#pragma once

#include "concepts.hpp"
#include "node_bitmap_traits.hpp"
#include "roaring/containers/array.h"
#include "roaring/containers/bitset.h"
#include "roaring/containers/containers.h"
#include "roaring/roaring_types.h"
#include <absl/container/flat_hash_map.h>
#include <gmp.h>
#include <omp.h>
#include <oneapi/tbb/parallel_sort.h>
#include <roaring/roaring.hh>
#include <stdexcept>



//
// FlatHashBitmap
// An implementation of the roaring bitmap for our NodeBitmap, using an Asbeil flat hashmap for O(1) performance.
//
template<AnySupportedIntegral T>
class FlatHashBitmapImpl {
    private:
    using Traits = BitmapKeyTraits<T>;
    using prefix_t = Traits::prefix_t;
    using suffix_t = Traits::suffix_t;
    using roaring_key_t = Traits::roaring_key_t;
    using roaring_value_t = Traits::roaring_value_t;
    using roaring_typecode_t = Traits::roaring_typecode_t;
    using roaring_word_t = Traits::roaring_word_t;
    using map_t = std::conditional_t<
        BuiltinIntegral<T>
        , absl::flat_hash_map<prefix_t, roaring::Roaring>
        , absl::flat_hash_map<prefix_t, roaring::Roaring, absl::Hash<prefix_t>, MpzEq>
    >;
    map_t _flat_map;
    std::vector<prefix_t> _sorted_prefixes;



    //
    // Add Prefix Key
    // Adds a prefix key to the private vector and sorts it.
    //
    void add_prefix_key(const prefix_t& prefix) {
        auto it = std::lower_bound(_sorted_prefixes.begin(), _sorted_prefixes.end(), prefix);
        // Check for duplicates and only insert if there are none.
        if (it == _sorted_prefixes.end() || *it != prefix) {
            _sorted_prefixes.insert(it, prefix);
        }
    }



    //
    // Remove Prefix Key
    // Removes a prefix key when the associated bitmap is empty after a remove() call.
    //
    void remove_prefix_key(const prefix_t& prefix) {
        auto it = std::lower_bound(_sorted_prefixes.begin(), _sorted_prefixes.end(), prefix);
        if (it != _sorted_prefixes.end() && *it == prefix) {
            _sorted_prefixes.erase(it);
        }
    }



    public:
    FlatHashBitmapImpl() = default;



    //
    // Get Map
    // Return the underlying map for caller to access.  Read-only.
    //
    // If you're using this directly, it should probably be an encapsulated method or you should use a for_each_*().
    //
    const map_t& get_map() const { return _flat_map; }



    //
    // Add
    // Add a value to the bitmap.
    //
    void add(const T& value) {
        prefix_t prefix = Traits::get_prefix(value);
        suffix_t suffix = Traits::get_suffix(value);

        auto [it, inserted] = _flat_map.try_emplace(prefix);
        if (inserted) {
            add_prefix_key(prefix);
        }
        it->second.add(suffix);
    }



    //
    // Add Range
    // Add a contiguous range of values using the most optimized approach.  Only works well when the prefixes match.
    //
    // This is the HALF-OPEN range (max is excluded).
    //
    void add_range(const T& start, const T& end) {
        prefix_t start_prefix = Traits::get_prefix(start);
        suffix_t start_suffix = Traits::get_suffix(start);
        prefix_t end_prefix = Traits::get_prefix(end);
        suffix_t end_suffix = Traits::get_suffix(end);
        if (start_prefix == end_prefix) {
            // They all fall into the same prefix.  Call a single addRange() and then leave.
            auto [it, inserted] = _flat_map.try_emplace(start_prefix);
            if (inserted) {
                add_prefix_key(start_prefix);
            }
            it->second.addRange(start_suffix, end_suffix);
        } else {
            // Insert the remainder of prefix_1 which clearly goes to SUFFIX_MAX since prefix_2 is beyond it.
            // Since the suffix is full, we use addRangeClosed() to encapsulate it.
            auto [first_it, first_inserted] = _flat_map.try_emplace(start_prefix);
            if (first_inserted) {
                add_prefix_key(start_prefix);
            }
            first_it->second.addRangeClosed(start_suffix, Traits::SUFFIX_MAX);

            // Loop through any intermediary prefixes, which are incremental of course between prefix_start and prefix_end, non-inclusive.
            // Again, since the suffix is guaranteed full, we use addRangeClosed() to encapsulate it.
            for (prefix_t middle_prefix = start_prefix + 1; middle_prefix < end_prefix; middle_prefix++) {
                auto [mid_it, mid_inserted] = _flat_map.try_emplace(middle_prefix);
                if (mid_inserted) {
                    add_prefix_key(middle_prefix);
                }
                mid_it->second.addRangeClosed(0, Traits::SUFFIX_MAX);
            }

            // Add the remaining bits to the last prefix, ending at "end" of course.
            // Reminder, we do NOT include the last value (end_suffix) because this is the half-open variant.
            auto [last_it, last_inserted] = _flat_map.try_emplace(end_prefix);
            if (last_inserted) {
                add_prefix_key(end_prefix);
            }
            last_it->second.addRange(0, end_suffix);
        }
    }
    //
    // Now we add the CLOSED range (max is included).
    //
    void add_range_closed(const T& start, const T& end) {
        // Just call the add_range() and throw the end on after-the-fact.
        add_range(start, end);
        add(end);
    }



    //
    // Contains
    // Check for a value to exist in the bitmap.
    //
    bool contains(const T& value) const {
        prefix_t prefix = Traits::get_prefix(value);
        suffix_t suffix = Traits::get_suffix(value);

        auto it = _flat_map.find(prefix);
        if (it == _flat_map.end()) {
            return false;
        }

        return it->second.contains(suffix);
    }



    //
    // Remove
    // Removes an item from the bitmap.
    //
    void remove(const T& value) {
        prefix_t prefix = Traits::get_prefix(value);
        suffix_t suffix = Traits::get_suffix(value);

        auto it = _flat_map.find(prefix);
        if (it == _flat_map.end()) {
            return;
        }
        it->second.remove(suffix);

        if (it->second.isEmpty()) {
            _flat_map.erase(it);
            remove_prefix_key(prefix);
        }
    }



    //
    // Cardinality (count)
    // Return the cardinality (count) of nodes on.
    //
    T cardinality() const {
        T total = 0;
        for (const auto& [prefix, bitmap] : _flat_map) {
            if constexpr(BuiltinIntegral<T>) {
                total += bitmap.cardinality;
            } else {
                mpz_add_ui(total.get_mpz_t(), total.get_mpz_t(), bitmap.cardinality);
            }
        }
        return total;
    }



    //
    // Clear
    // Removes all items from all bitmaps by clearing the entire hash object.
    //
    void clear() {
        _flat_map.clear();
        _sorted_prefixes.clear();
    }



    //
    // Merge
    // Merge (bitwise OR) all source bitmaps into self.
    //
    void merge(const FlatHashBitmapImpl<T>& src) {
        for (const auto& [src_prefix, src_bitmap] : src.get_map()) {
            // Get an iterator to our keypair.
            auto [it, inserted] = _flat_map.try_emplace(src_prefix);
            // Add the prefix if it was missing.
            if (inserted) {
                add_prefix_key(src_prefix);
            }
            // Merge using bitwise |=.
            it->second |= src_bitmap;
        }
    }
    //
    // Mimic the operator.
    void operator|=(const FlatHashBitmapImpl<T>& src) {
        (*this)->merge(src);
    }



    //
    // Clone
    // Copy source bitmap and metadata into self, making an exact copy.
    //
    void clone(const FlatHashBitmapImpl<T>& src) {
        (*this)->clear();
        for (const auto& [src_prefix, src_bitmap] : src.get_map()) {
            // Add to the prefix key list.
            add_prefix_key(src_prefix);
            // Build the key on our map.
            _flat_map.try_emplace(src_prefix);
            auto [it, last_inserted] = _flat_map.try_emplace(src_prefix);
            // Clone the src to our new bitmap using the overwrite method.
            bool successful = roaring::api::roaring_bitmap_overwrite(it->second, src_bitmap);
            if (!successful) {
                throw std::runtime_error("Unable to clone a bitmap using roaring_bitmap_overwite when calling clone().");
            }
        }
    }



    //
    // Optimize
    // Calls the internal runOptimize() for each roaring map, which (I think) is an RLE analysis and/or compaction task.
    //
    void optimize() {
        for (auto& [key, bitmap] : _flat_map) {
            bitmap.runOptimize();
        }
    }



    //
    // Shrink To Fit
    // Calls the shrinkToFit() method for each bitmap.  Sums the savings and returns it, since the API does too.
    //
    size_t shrink_to_fit() {
        size_t total = 0;
        for (auto& [key, bucket] : _flat_map) {
            total += bucket.shrinkToFit();
        }
        return total;
    }



    //
    // Deep Size
    // Calculate the size of this data structure as closely as possible.
    //
    // This is NOT cheap.  It requires iteration of the entire structure.
    //
    size_t deep_size() const {
        size_t total = 0;
        total += sizeof(*this);

        // Add the map's internal storage.  Should be size of a pair of <T,Roaring> and a byte of metadata.
        total += _flat_map.capacity() * (sizeof(std::pair<const prefix_t, roaring::Roaring>) + 1);

        // Add the size of the roaring objects.
        for(const auto& pair : _flat_map) {
            total += pair.second.getSizeInBytes(false);
        }

        return total;
    }



    //
    // For-Each Value
    // This a convenience wrapper, designed when you don't need 'tls' storage.
    //
    // Applies callback to all values.  Your callback must have this signature:
    //   (const T& value)
    //
    // The 'value' is the reconstructed value from the bitmap and must be const to avoid GMP alloc() temps.
    //
    // You may NOT modify this bitmap while operating!  We rely on CRoaring's iterators and internal structures which are
    // invalidated upon changes!
    //
    template<typename Func>
    void for_each_value(BitmapTransformerPolicy policy, Func&& callback) {
        // Do not allow non-ref callbacks.  Otherwise we make GMP over and over.
        static_assert(std::is_same_v<typename first_arg_type<Func>::type, const T&>, "Callback must be callable with a const T&");
        std::vector<uint8_t> dummy_tls;
        for_each_transformer(policy, dummy_tls, [&](const T& value, auto&) {
            return callback(value);
        });
    }




    //
    // For-Each Transformer
    // Applies callback to all values according to BitmapTransformerPolicy (serial or parallel).  Callback must have this signature:
    //   (const T& value, TLS_Type& tls)
    //
    // The 'value' is the reconstructed value from the bitmap and must be const to avoid GMP alloc() temps.
    // The 'tls' is a vector YOU send and an element will be returned in callback().
    //   - We will use the omp_get_thread_num() as your vector's element ID.
    //   - We will call tls.resize() if omp_get_max_threads() > tls.capacity().
    //
    // You may NOT modify THIS bitmap while operating!  We rely on CRoaring's iterators and internal structures which are
    // invalidated upon changes!  If you need bitmap changes, include bitmaps in your tls& and .merge() them afterward.
    //
    template<typename Func, typename TLS_Type>
    void for_each_transformer(BitmapTransformerPolicy policy, std::vector<TLS_Type>& tls, Func&& callback) {
        // Do not allow non-ref callbacks.  Otherwise we make GMP over and over.
        static_assert(std::is_same_v<typename first_arg_type<Func>::type, const T&>, "Callback must be callable with a const T&");

        // Ensure the TLS has enough elements before proceeding.
        const size_t max_threads = policy == BitmapTransformerPolicy::SERIAL ? 1 : static_cast<size_t>(omp_get_max_threads());
        if (tls.size() < max_threads) {
            tls.resize(max_threads);
        }

        // Setup our control-stop.
        bool stop;

        // Process according to policy.
        if (policy == BitmapTransformerPolicy::SERIAL) {
            // Serial Path
            // Guarantees sequential processing.  Can efficiently use exposed iterators.

            // Hoist these, just in case of temp allocs.
            T value;
            T shifted_prefix;
            suffix_t suffix;

            // Even though we're not threaded, get our thread ID for clarity.
            size_t my_thread_id = static_cast<size_t>(omp_get_thread_num());

            // The TLS should only need index 0.
            TLS_Type my_tls = tls[my_thread_id];

            // Loop through prefixes in order.
            for (const prefix_t& prefix : _sorted_prefixes) {
                // Shift the prefix once instead of deeper inside the while() loop.
                if constexpr(BuiltinIntegral<T>) {
                    shifted_prefix = static_cast<T>(prefix) << Traits::SUFFIX_BITS;
                } else {
                    mpz_mul_2exp(shifted_prefix.get_mpz_t(), prefix.get_mpz_t(), Traits::SUFFIX_BITS);
                }

                // Find our bitmap and connect an iterator.
                roaring::Roaring& roaring_obj = _flat_map.find(prefix)->second;
                roaring::api::roaring_uint32_iterator_t bitmap_iterator;
                roaring::api::roaring_iterator_init(&roaring_obj.roaring, &bitmap_iterator);

                // Loop this iterator until we're done.  Use addition instead of bitwise-OR (|) to avoid alloc on GMP path.
                while (bitmap_iterator.has_value) {
                    suffix = bitmap_iterator.current_value;
                    if constexpr(BuiltinIntegral<T>) {
                        value = shifted_prefix + suffix;
                    } else {
                        mpz_add_ui(value.get_mpz_t(), shifted_prefix.get_mpz_t(), suffix);
                    }
                    stop = callback(value, my_tls);
                    if (stop) { break; }
                    roaring::api::roaring_uint32_iterator_advance(&bitmap_iterator);
                }

                // If stopping, break from the prefix for() loop too.
                if (stop) { break; }
            }
        } else {
            // Parallel Path
            // Work is threaded, breaking sequential guarantee.  Uses prefix OR range iteration for performance.
            //
            // An enticing key to split work by would be the prefix keys.  Sadly, there are 2^32 nodes beneath each prefix.  This
            // prevents ANY multithreading until we have at least >2^32 nodes (i.e.: 2+ prefixes) and we don't achieve full
            // multithreading until we have at least one prefix per OMP thread.
            //
            // It gets worse.  Even with enough prefixes, the density can vary greatly amongst them, affecting runtime when threads
            // synchronize at the end.
            //
            // One solution is to iterate with OMP over the containers themselves by exploiting the CRoaring internals.  While safe
            // in theory, it's convoluted and, more importantly, OMP scheduling overhead (startup/teardown) is expensive at this
            // scale.  (Note: we left a theoretical container-scanning approach commented below.)
            //
            // To resolve this, we chose a middle ground approach.  We let each thread scan a range of each prefix, determined by
            // its thread_id.  This is made possible because roaring_uint32_iterator_move_equalorlarger() exists and is extremely
            // cheap.
            //
            // We left a theoretical example of per-container processing, but honestly the
            //
            //
            //   When there are more prefixes than threads available to OMP,
            // we will distribute entire prefixes to a thread using a dynamic scheduler and iterate with an API-safe iterator.
            //
            // Sadly, each has prefix contains 2^32 nodes represented, which immediately limits parallelization.  Therefore, when
            // the number of prefixes is less than the number of threads available to OMP, we will iterate over each prefix like
            // above, but ask each thread to scan forward and process its own range.
            //
            //   Therefore, when the number of prefixes is less than the
            // number of threads available to OMP, we will iterate over each prefix serailly and thread OMP over the containers.
            //
            // When the prefix count is higher than the number of threads availble to OMP, it's faster to split the work using a
            // dynamic scheduler over the prefixes themselves.  Each prefix has a domain of 2^32 possible nodes beneath it.
            //
            // When the prefix count is lower than the number of threads available to OMP, it's faster to split the work using a
            // dynamic scheduler over the containers within prefixes.



            // Loop through each prefix.  We pay threading cost per-prefix, but I believe the workload in the callback warrants it.
            for (size_t prefix_index = 0; prefix_index < _sorted_prefixes.size(); prefix_index++) {
                #pragma omp parallel default(none) shared(prefix_index)
                {
                    // Get our thread ID.
                    size_t my_thread_id = static_cast<size_t>(omp_get_thread_num());

                    // Pick our element from the TLS.
                    TLS_Type my_tls = tls[my_thread_id];

                    // Grab the prefix and then build a shifted_prefix so we don't recompute it.
                    const prefix_t& prefix = _sorted_prefixes[prefix_index];
                    T shifted_prefix;
                    if constexpr(BuiltinIntegral<T>) {
                        shifted_prefix = static_cast<T>(prefix) << Traits::SUFFIX_BITS;
                    } else {
                        mpz_mul_2exp(shifted_prefix.get_mpz_t(), prefix.get_mpz_t(), Traits::SUFFIX_BITS);
                    }

                    // Find our bitmap and connect to the internal storage components.
                    auto it = _flat_map.find(prefix);
                    const roaring::api::roaring_bitmap_t* roaring_bitmap = &(it->second.roaring);
                    const roaring::api::roaring_array_t* high_low_container = &(roaring_bitmap->high_low_container);

                    // Hoist a few locals to avoid lots of potential alloc on GMP path.
                    T value;
                    T shifted_prefix_and_key;

                    // Build the inner loop around the keys and sub containers.  Split it up with OMP.
                    #pragma omp for schedule(dynamic)
                    for (int high_low_index = 0; high_low_index < high_low_container->size; high_low_index++) {
                        // Grab the roaring key, the container, and type code.
                        roaring_key_t suffix_key = high_low_container->keys[high_low_index];
                        const roaring::api::container_s* container = high_low_container->containers[high_low_index];
                        roaring_typecode_t typecode = high_low_container->typecodes[high_low_index];

                        // Hoist and promote+shift the key into full suffix_t space.
                        suffix_t shifted_key = (suffix_t(suffix_key)) << Traits::ROARING_KEY_BITS;
                        suffix_t suffix_val;

                        // Build the shifted prefix and key combo so we don't repeat it below.
                        if constexpr(BuiltinIntegral<T>) {
                            shifted_prefix_and_key = shifted_prefix + shifted_key;
                        } else {
                            mpz_add_ui(shifted_prefix_and_key.get_mpz_t(), shifted_prefix.get_mpz_t(), shifted_key);
                        }

                        // Select the correct iteration method based on the typecode to retrieve and process suffixes.
                        // Use addition instead of bitwise-OR (|) to avoid alloc on GMP path.
                        switch (typecode) {
                            case ARRAY_CONTAINER_TYPE: {
                                const roaring::internal::array_container_t* array_container = (roaring::internal::array_container_t*)container;
                                for (int array_index = 0; array_index < array_container->cardinality; array_index++) {
                                    suffix_val = array_container->array[array_index];
                                    if constexpr(BuiltinIntegral<T>) {
                                        value = shifted_prefix_and_key + suffix_val;
                                    } else {
                                        mpz_add_ui(value.get_mpz_t(), shifted_prefix_and_key.get_mpz_t(), suffix_val);
                                    }
                                    stop = callback(value, my_tls);
                                    if (stop) { break; }
                                }
                                break;
                            }

                            case BITSET_CONTAINER_TYPE: {
                                const roaring::internal::bitset_container_t* bitset_container = (const roaring::internal::bitset_container_t*)container;
                                for (int word_index = 0; word_index < roaring::internal::BITSET_CONTAINER_SIZE_IN_WORDS; word_index++) {
                                    roaring_word_t word = bitset_container->words[word_index];
                                    // Loop until there are no more on-bits in the word.
                                    while (word != 0) {
                                        // Calculate the offset by counting the number of trailing zeros, giving us the next "1" position.
                                        int offset = __builtin_ctzll(word);
                                        // Multiply out the word index by the number of bits per word, then add the offset.  That's the next "on" node.
                                        suffix_val = (word_index * Traits::ROARING_WORD_BITS) + offset;
                                        if constexpr(BuiltinIntegral<T>) {
                                            value = shifted_prefix_and_key + suffix_val;
                                        } else {
                                            mpz_add_ui(value.get_mpz_t(), shifted_prefix_and_key.get_mpz_t(), suffix_val);
                                        }
                                        stop = callback(value, my_tls);
                                        if (stop) { break; }
                                        // Now bitwise-AND the word minus one to wipe out the least-significant 1s place which we just processed.
                                        word &= (word - 1);
                                    }
                                    // Leave the for() loop if we're stopping early.
                                    if (stop) { break; }
                                }
                                break;
                            }

                            case RUN_CONTAINER_TYPE: {
                                const roaring::internal::run_container_t* run_container = (const roaring::internal::run_container_t*)container;
                                for (int run_index = 0; run_index < run_container->n_runs; run_index++) {
                                    roaring_value_t start = run_container->runs[run_index].value;
                                    roaring_value_t length = run_container->runs[run_index].length;
                                    suffix_t end = start + length;
                                    for (suffix_val = start; suffix_val <= end; suffix_val++) {
                                        if constexpr(BuiltinIntegral<T>) {
                                            value = shifted_prefix_and_key + suffix_val;
                                        } else {
                                            mpz_add_ui(value.get_mpz_t(), shifted_prefix_and_key.get_mpz_t(), suffix_val);
                                        }
                                        stop = callback(value, my_tls);
                                        if (stop) { break; }
                                    }
                                    // Break from the outer for() loop if we're stopping.
                                    if (stop) { break; }
                                }
                                break;
                            }
                        }
                        // Break the OMP for() loop if we're stopping.  OMP doesn't support "break", so we set the index to its sentinal value.
                        if (stop) { high_low_index = high_low_container->size; }
                    } // end omp for loop
                } // end omp parallel region
                // Break the prefix loop if we're stopping.
                if (stop) { break; }
            } // end for prefix_index
        } // end if serial/parallel
    }

};
