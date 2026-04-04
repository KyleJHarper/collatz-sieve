#pragma once

#include "concepts.hpp"
#include "node_bitmap_traits.hpp"
#include "roaring/containers/array.h"
#include "roaring/containers/bitset.h"
#include "roaring/containers/containers.h"
#include "roaring/roaring_types.h"
#include <absl/container/flat_hash_map.h>
#include <atomic>
#include <gmp.h>
#include <omp.h>
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
        // Prefix can be mpz_class, so use TLS and out&.
        static thread_local prefix_t prefix;
        if constexpr(BuiltinIntegral<T>) {
            prefix = Traits::get_prefix(value);
        } else {
            Traits::get_prefix(value, prefix);
        }
        // Suffix is just a uint32_t.
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
        // Prefix can be mpz_class, so use TLS and out&.
        static thread_local prefix_t start_prefix;
        static thread_local prefix_t end_prefix;
        if constexpr(BuiltinIntegral<T>) {
            start_prefix = Traits::get_prefix(start);
            end_prefix = Traits::get_prefix(end);
        } else {
            Traits::get_prefix(start, start_prefix);
            Traits::get_prefix(end, end_prefix);
        }
        // Suffix is just a uint32_t.
        suffix_t start_suffix = Traits::get_suffix(start);
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
        // Prefix can be mpz_class, so use TLS and out&.
        static thread_local prefix_t prefix;
        if constexpr(BuiltinIntegral<T>) {
            prefix = Traits::get_prefix(value);
        } else {
            Traits::get_prefix(value, prefix);
        }
        // Suffix is just a uint32_t.
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
        // Prefix can be mpz_class, so use TLS and out&.
        static thread_local prefix_t prefix;
        if constexpr(BuiltinIntegral<T>) {
            prefix = Traits::get_prefix(value);
        } else {
            Traits::get_prefix(value, prefix);
        }
        // Suffix is just a uint32_t.
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
                total += bitmap.cardinality();
            } else {
                mpz_add_ui(total.get_mpz_t(), total.get_mpz_t(), bitmap.cardinality());
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
    FlatHashBitmapImpl<T>& operator|=(const FlatHashBitmapImpl<T>& src) {
        merge(src);
        return *this;
    }



    //
    // Clone
    // Copy source bitmap and metadata into self, making an exact copy.
    //
    void clone(const FlatHashBitmapImpl<T>& src) {
        clear();
        for (const auto& [src_prefix, src_bitmap] : src.get_map()) {
            // Add to the prefix key list.
            add_prefix_key(src_prefix);
            // Build the key on our map.
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
    // Note, we already check for empty in remove(), so dead prefixes are taken care of.
    //
    void optimize() {
        for (auto& [prefix, bitmap] : _flat_map) {
            bitmap.runOptimize();
        }
    }



    //
    // Shrink To Fit
    // Calls the shrinkToFit() method for each bitmap.  Sums the savings and returns it, since the API does too.
    //
    size_t shrink_to_fit() {
        size_t total = 0;
        for (auto& [prefix, bitmap] : _flat_map) {
            total += bitmap.shrinkToFit();
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
        static_assert(std::is_invocable_v<Func, const T&>, "Callback must be callable with (const T&)");
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
        static_assert(std::is_invocable_r_v<bool, Func, const T&, TLS_Type&>, "Callback must be callable as void(const T&, TLS_Type&)");

        // Ensure the TLS has enough elements before proceeding.
        const size_t max_threads = policy == BitmapTransformerPolicy::SERIAL ? 1 : static_cast<size_t>(omp_get_max_threads());
        if (tls.size() < max_threads) {
            tls.resize(max_threads);
        }

        // Setup our control-stop.
        std::atomic<bool> atomic_stop = false;
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
            TLS_Type& my_tls = tls[my_thread_id];

            // Loop through prefixes in order.
            for (const prefix_t& prefix : _sorted_prefixes) {
                // Shift the prefix once instead of deeper inside the while() loop.  For smaller types, set to 0.
                if constexpr(sizeof(T) > Traits::SUFFIX_BYTES) {
                    // T can hold the prefix.  Let it shift.
                    if constexpr(BuiltinIntegral<T>) {
                        shifted_prefix = static_cast<T>(prefix) << Traits::SUFFIX_BITS;
                    } else {
                        mpz_mul_2exp(shifted_prefix.get_mpz_t(), prefix.get_mpz_t(), Traits::SUFFIX_BITS);
                    }
                } else {
                    // We have a T smaller than suffix_t, which means the only prefix we'll ever have is 0.
                    // Set it manually, because small-T means we can't shift SUFFIX_BITS wide.
                    shifted_prefix = 0;
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
            // Work is threaded, breaking sequential guarantee.  Uses CRoaring internals for performance.
            //
            // While throwing _sorted_prefixes at OMP might seem ideal, it would result in only a single thread working until level
            // 32.  This is because CRoaring uses a 32-bit map (16 bit key, 16 bit suffix) internally.
            //
            // As such, we will read the internal high-low containers and iterate based on their type (array, bitmap, RLE).

            // Open the parallel region early and establish thread-locals.
            #pragma omp parallel default(none) shared(tls, atomic_stop, callback)
            {
                // Get our thread ID.
                size_t my_thread_id = static_cast<size_t>(omp_get_thread_num());

                // Pick our element from the TLS.
                TLS_Type& my_tls = tls[my_thread_id];

                // Track a local-stop.
                bool local_stop = false;

                // Hoist a few locals to avoid lots of potential alloc on GMP path.
                T value;
                T shifted_prefix;
                T shifted_prefix_and_key;

                // Loop through prefixes.
                for (size_t prefix_index = 0; prefix_index < _sorted_prefixes.size(); prefix_index++) {
                    // Stop if needed.
                    if (atomic_stop.load(std::memory_order_relaxed)) { continue; }

                    // Grab the prefix and then build a shifted_prefix so we don't recompute it.
                    const prefix_t& prefix = _sorted_prefixes[prefix_index];
                    // Shift the prefix once instead of deeper inside the while()/for() loops.  For smaller types, set to 0.
                    if constexpr(sizeof(T) > Traits::SUFFIX_BYTES) {
                        // T can hold the prefix.  Let it shift.
                        if constexpr(BuiltinIntegral<T>) {
                            shifted_prefix = static_cast<T>(prefix) << Traits::SUFFIX_BITS;
                        } else {
                            mpz_mul_2exp(shifted_prefix.get_mpz_t(), prefix.get_mpz_t(), Traits::SUFFIX_BITS);
                        }
                    } else {
                        // We have a T smaller than suffix_t, which means the only prefix we'll ever have is 0.
                        // Set it manually, because small-T means we can't shift SUFFIX_BITS wide.
                        shifted_prefix = 0;
                    }

                    // Find our bitmap and connect to the internal storage components.
                    auto it = _flat_map.find(prefix);
                    const roaring::api::roaring_bitmap_t* roaring_bitmap = &(it->second.roaring);
                    const roaring::api::roaring_array_t* high_low_container = &(roaring_bitmap->high_low_container);
                    // Hoist container bits.  Probably optimized away, but meh.
                    const auto* high_low_container__keys = high_low_container->keys;
                    const auto* high_low_container__containers = high_low_container->containers;
                    const auto* high_low_container__typecodes = high_low_container->typecodes;

                    // Build the inner loop around the keys and sub containers.  Split it up with OMP.
                    #pragma omp for schedule(dynamic)
                    for (int high_low_index = 0; high_low_index < high_low_container->size; high_low_index++) {
                        // Check for stop again at the chunk level.
                        if (atomic_stop.load(std::memory_order_relaxed)) { continue; }

                        // Grab the roaring key, the container, and type code.
                        roaring_key_t suffix_key = high_low_container__keys[high_low_index];
                        const roaring::api::container_s* container = high_low_container__containers[high_low_index];
                        roaring_typecode_t typecode = high_low_container__typecodes[high_low_index];

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
                                    local_stop = callback(value, my_tls);
                                    if (local_stop) {
                                        atomic_stop.store(true, std::memory_order_relaxed);
                                        break;
                                    }
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
                                        local_stop = callback(value, my_tls);
                                        if (local_stop) {
                                            atomic_stop.store(true, std::memory_order_relaxed);
                                            break;
                                        }
                                        // Now bitwise-AND the word minus one to wipe out the least-significant 1s place which we just processed.
                                        word &= (word - 1);
                                    }
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
                                        local_stop = callback(value, my_tls);
                                        if (local_stop) {
                                            atomic_stop.store(true, std::memory_order_relaxed);
                                            break;
                                        }
                                    }
                                }
                                break;
                            }
                        }
                    } // end omp for loop
                } // end for prefix_index
            } // end omp parallel region
        } // end if serial/parallel
    }

};
