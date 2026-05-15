#pragma once

#include "concepts.hpp"
#include "string.hpp"
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
#include <string>
#include "gmp.hpp"
#include "stream_helper.hpp"
#include "equality_helper.hpp"




//
// FlatHashBitmap
//
//
/**
* @class FlatHashBitmapImpl
* @brief An implementation of the roaring bitmap for our NodeBitmap, using an Asbeil flat hashmap.
* @note This class uses CRoaring under the hood, and does its best to match function names to be reasonably similar in nature.
* @note This class is an Impl, and not meant to be used directly.  Use the `NodeBitmap` facade.
* @tparam T Any supported integral (see concepts.hpp).
*/
template<AnySupportedIntegral T>
class FlatHashBitmapImpl {
    private:
    using Traits = BitmapKeyTraits<T>;                       ///< Taken from `BitmapKeyTraits`.  See node_bitmap_traits.hpp.
    using prefix_t = Traits::prefix_t;                       ///< Taken from `BitmapKeyTraits`.  See node_bitmap_traits.hpp.
    using suffix_t = Traits::suffix_t;                       ///< Taken from `BitmapKeyTraits`.  See node_bitmap_traits.hpp.
    using roaring_key_t = Traits::roaring_key_t;             ///< Taken from `BitmapKeyTraits`.  See node_bitmap_traits.hpp.
    using roaring_value_t = Traits::roaring_value_t;         ///< Taken from `BitmapKeyTraits`.  See node_bitmap_traits.hpp.
    using roaring_typecode_t = Traits::roaring_typecode_t;   ///< Taken from `BitmapKeyTraits`.  See node_bitmap_traits.hpp.
    using roaring_word_t = Traits::roaring_word_t;           ///< Taken from `BitmapKeyTraits`.  See node_bitmap_traits.hpp.
    /**
    * @brief Defines a map type to help cover all necessary components of an `absl::flat_hash_map`.
    *
    * The following categories apply:
    *   1. When `T` is a fixed-width integral, Absl can hash it directly, so nothing special is required.
    *   2. When `T` is anything else, it's assumed to be `mpz_class`, and both `absl::Hash<prefix_t>` and `MpzEq` are needed.
    */
    using map_t = std::conditional_t<
        FixedWidthIntegral<T>
        , absl::flat_hash_map<prefix_t, roaring::Roaring>
        , absl::flat_hash_map<prefix_t, roaring::Roaring, absl::Hash<prefix_t>, GMP::MpzEq>
    >;
    /// @brief The `absl::flat_hash_map` typed to `prefix_t` and a `roaring::Roaring` bitmap.
    map_t _flat_map;
    /// @brief A synchronized list of prefixes.
    std::vector<prefix_t> _sorted_prefixes;



    /// @brief Adds a prefix key to the private vector and sorts it.
    void add_prefix_key(const prefix_t& prefix) {
        auto it = std::lower_bound(_sorted_prefixes.begin(), _sorted_prefixes.end(), prefix);
        // Check for duplicates and only insert if there are none.
        if (it == _sorted_prefixes.end() || *it != prefix) {
            _sorted_prefixes.insert(it, prefix);
        }
    }



    /// @brief Removes a prefix key when the associated bitmap is empty after a remove() call.
    void remove_prefix_key(const prefix_t& prefix) {
        auto it = std::lower_bound(_sorted_prefixes.begin(), _sorted_prefixes.end(), prefix);
        if (it != _sorted_prefixes.end() && *it == prefix) {
            _sorted_prefixes.erase(it);
        }
    }



    public:
    /// @name Lifecycle Management
    /// @{

    /// @brief Default constructor
    FlatHashBitmapImpl() = default;



    /// @brief Disallow copying.
    FlatHashBitmapImpl(const FlatHashBitmapImpl&) = delete;
    /// @brief Disallow copying.
    FlatHashBitmapImpl& operator=(const FlatHashBitmapImpl&) = delete;



    /// @brief Moving is okay.
    FlatHashBitmapImpl(FlatHashBitmapImpl&&) noexcept = default;
    /// @brief Moving is okay.
    FlatHashBitmapImpl& operator=(FlatHashBitmapImpl&&) noexcept = default;



    /// @brief Removes all items from all bitmaps by clearing the entire hash object.
    void clear() {
        _flat_map.clear();
        _sorted_prefixes.clear();
    }

    /// @}



    /// @name Accessors
    /// @{

    /// @brief Get a readonly reference to the internal flat map (the Abseil object).
    const map_t& get_map() const { return _flat_map; }



    /// @brief Return the underlying sorted prefixes vector.  Read-only.
    const std::vector<prefix_t>& get_sorted_prefixes() const { return _sorted_prefixes; }

    /// @}



    /// @brief Add a value to the bitmap.
    void add(const T& value) {
        // Prefix can be mpz_class, so use TLS and out&.
        static thread_local prefix_t prefix;
        if constexpr(FixedWidthIntegral<T>) {
            prefix = Traits::get_prefix(value);
        } else if constexpr(GMPIntegral<T>) {
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



    /**
    * @brief Add a contiguous range of values using the most optimized approach.
    * @note This is the Half-Open range [star, end).  Meaning, start is included but end isn't.
    */
    void add_range(const T& start, const T& end) {
        // Prefix can be mpz_class, so use TLS and out&.
        static thread_local prefix_t start_prefix;
        static thread_local prefix_t end_prefix;
        if constexpr(FixedWidthIntegral<T>) {
            start_prefix = Traits::get_prefix(start);
            end_prefix = Traits::get_prefix(end);
        } else if constexpr(GMPIntegral<T>) {
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



    /**
    * @brief Add a closed, contiguous range of values using the most optimized approach.
    * @note This is the Closed range [star, end].  Meaning, start and end are both included.
    */
    void add_range_closed(const T& start, const T& end) {
        // Just call the add_range() and throw the end on after-the-fact.
        add_range(start, end);
        add(end);
    }



    /**
    * @brief Check for a value to exist (be "on") in the bitmap.
    * @param value The value to search for.
    * @return True if present ("on"), false otherwise.
    */
    bool contains(const T& value) const {
        // Prefix can be mpz_class, so use TLS and out&.
        static thread_local prefix_t prefix;
        if constexpr(FixedWidthIntegral<T>) {
            prefix = Traits::get_prefix(value);
        } else if constexpr(GMPIntegral<T>) {
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



    /// @brief Removes an item from the bitmap.
    void remove(const T& value) {
        // Prefix can be mpz_class, so use TLS and out&.
        static thread_local prefix_t prefix;
        if constexpr(FixedWidthIntegral<T>) {
            prefix = Traits::get_prefix(value);
        } else if constexpr(GMPIntegral<T>) {
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



    /**
    * @brief Calculate the cardinality (count) of nodes turned "on".
    * @return The count of "on" nodes, typed to caller's `T`.
    */
    T cardinality() const {
        T total = 0;
        for (const auto& [prefix, bitmap] : _flat_map) {
            if constexpr(FixedWidthIntegral<T>) {
                total += bitmap.cardinality();
            } else if constexpr(GMPIntegral<T>) {
                mpz_add_ui(total.get_mpz_t(), total.get_mpz_t(), bitmap.cardinality());
            }
        }
        return total;
    }



    /**
    * @brief Merge (bitwise OR) another `FlatHashBitmapImpl` into this one.
    * @param src Another `FlatHashBitmapImpl` of the exact same `T`.
    */
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



    /**
    * @brief An opertor which calls `merge()`.  This is a convenience to do: `my_bitmap |= another_bitmap`.
    * @note Unlike `merge()`, caller receives `*this` returned, not void.
    * @return This object, as required by `operator|=`.
    */
    FlatHashBitmapImpl<T>& operator|=(const FlatHashBitmapImpl<T>& src) {
        merge(src);
        return *this;
    }



    /// @brief Copy source bitmap and metadata into self, making an exact copy.
    void clone(const FlatHashBitmapImpl<T>& src) {
        clear();
        for (const auto& [src_prefix, src_bitmap] : src.get_map()) {
            // Add to the prefix key list.
            add_prefix_key(src_prefix);
            // Build the key on our map.
            auto [it, last_inserted] = _flat_map.try_emplace(src_prefix);
            // Clone the src to our new bitmap using copy semantics.
            it->second = src_bitmap;
        }
    }



    /**
    * @brief Calls the internal runOptimize() for each roaring map, which is an RLE analysis and/or compaction task.
    * @note Empty sets are already checked for in `remove()`, so no dead prefixes exist in `_sorted_prefixes`.
    */
    void optimize() {
        for (auto& [prefix, bitmap] : _flat_map) {
            bitmap.runOptimize();
        }
    }



    /**
    * @brief Calls the shrinkToFit() method for each bitmap and sums them.
    * @returns The amount of space reclaimed, because that's what CRoaring's API does too.
    */
    size_t shrink_to_fit() {
        size_t total = 0;
        for (auto& [prefix, bitmap] : _flat_map) {
            total += bitmap.shrinkToFit();
        }
        return total;
    }



    /**
    * @brief Calculate the size of this data structure as closely as possible.
    * @note This is not always cheap.  It requires iteration of all structures.
    * @return The size in bytes of the object.
    */
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



    /**
    * @brief Compare two flat hash bitmaps' specific internals and return true if identical.
    *
    * This method currently does everything, because it's the only implementation behind the facade, so there's nohting to
    * generalize.
    *
    * This function checks:
    *   1. Prefixes count.
    *   2. Prefixes one-by-one, and the CRoaring bitmap associated withh each.
    *
    * @param first The first bitmap to compare.
    * @param second The second bitmap to compare.
    * @param err Pointer to a string where inequality or error messages are stored.
    * @return True if equal, false otherwise.
    */
    static bool st_equal(const FlatHashBitmapImpl<T>& first, const FlatHashBitmapImpl<T>& second, std::string* err = nullptr) {
        EqualityHelper eq(err);
        eq.set_category("FlatHashBitmapImpl");

        // Grab the flat hash maps and prefixes.
        const map_t& f_flat_map = first.get_map();
        const map_t& s_flat_map = second.get_map();
        const std::vector<prefix_t>& f_prefixes = first.get_sorted_prefixes();
        const std::vector<prefix_t>& s_prefixes = second.get_sorted_prefixes();
        // Check size.
        if (! eq.equal(f_prefixes.size(), s_prefixes.size())) {
            return eq.fail("Flat hash impl sorted_prefxies size mistmatch");
        }
        // Prefixes should match and are already sorted.  Test each prefix, and then the CRoaring bitmap beneath it.
        for (size_t i = 0; i < f_prefixes.size(); i++) {
            if (! eq.equal(f_prefixes.at(i), s_prefixes.at(i))) {
                return eq.fail("Flat hash prefix at ID " + to_string_any(i) + " mismatch");
            }
            const roaring::Roaring& f_roaring = f_flat_map.at(f_prefixes.at(i));
            const roaring::Roaring& s_roaring = s_flat_map.at(s_prefixes.at(i));
            if (! roaring::api::roaring_bitmap_equals(&(f_roaring.roaring), &(s_roaring.roaring))) {
                return eq.fail("Bitmaps do not match for prefix " + to_string_any(f_prefixes.at(i)));
            }
        }

        // Guess they're equal.
        return true;
    }



    /**
    * @brief Compare another bitmap to this one.
    *
    * This is a member helper which simply forwards to `FlatHashBitmapImpl::st_equal()`.
    *
    * @param second The second bitmap to compare against this.
    * @param err Pointer to a string where inequality or error messages are stored.
    * @return True if equal, false otherwise.
    */
    bool equal(const FlatHashBitmapImpl<T>& second, std::string* err = nullptr) const {
        return st_equal(*this, second, err);
    }



    /**
    * @brief Serialize the bitmap specifics of this object for export.
    *
    * Serialization happens in this order:
    *   1. Count of prefixes.
    *   2. Prefixes one-by-one, and then the associated CRoaring bitmap data.
    *
    * @note Since this is the only implementation, all data is dumped from here.
    * @note This method does not throw.
    * @param out The stream to write data to.
    * @param err Pointer to a string where errors, if any, are written.
    * @return A boolean indicating success or failure.  Do not discard.
    */
    [[nodiscard]] bool serialize(std::ostream& out, std::string* err = nullptr) const {
        StreamHelper sh(nullptr, &out, err);
        sh.set_category("FlatHashBitmapImpl");

        // Write out the number of prefixes/bitmaps.
        uint64_t u64_prefix_count = static_cast<uint64_t>(_sorted_prefixes.size());
        if (! sh.serialize_integral(u64_prefix_count)) {
            return sh.fail("prefix_count==" + to_string_any(u64_prefix_count));
        }

        // Now loop through each key-value pair.  Make sure to do them in order so exported files remain deterministic.
        for (const prefix_t& prefix : _sorted_prefixes) {
            // Write the prefix.
            if (! sh.serialize_integral(prefix)) {
                return sh.fail("prefix==" + to_string_any(prefix));
            }
            // Find the bitmap.
            auto it = _flat_map.find(prefix);
            if (it == _flat_map.end()) {
                return sh.fail("couldn't find bitmap for prefix==" + to_string_any(prefix));
            }
            const roaring::Roaring& bitmap = it->second;
            // Write how much space the bitmap will take.
            uint64_t u64_bitmap_size = static_cast<uint64_t>(bitmap.getSizeInBytes(true));
            if (! sh.serialize_integral(u64_bitmap_size)) {
                return sh.fail("bitmap_size==" + to_string_any(u64_bitmap_size));
            }
            // Build the buffer and flush it.
            std::vector<char> buffer(u64_bitmap_size);
            size_t actual_bytes = bitmap.write(buffer.data(), true);
            if (actual_bytes != u64_bitmap_size) {
                return sh.fail("Number of bytes to write expected by getSizeInBytes doesn't match actual bytes returned");
            }
            if (! sh.write_bytes(buffer.data(), u64_bitmap_size)) {
                return sh.fail("roaring bitmap data");
            }
        }

        // All good.
        return true;
    }



    /**
    * @brief Deserialize this object for import following a previous `serialize()`.
    *
    * Deserialization happens in the same order as serialization, obviously.
    *
    * @note This method does not throw.
    * @param in The stream to read data from.
    * @param err Pointer to a string where errors, if any, are written.
    * @return A boolean indicating success or failure.  Do not discard.
    */
    [[nodiscard]] bool deserialize(std::istream& in, std::string* err = nullptr) {
        StreamHelper sh(&in, nullptr, err);
        sh.set_category("FlatHashBitmapImpl");

        // Clear it out (reset).
        clear();

        // Prefixes
        uint64_t u64_prefix_count = 0;
        if (! sh.deserialize_integral(u64_prefix_count)) {
            return sh.fail("couldn't read prefix count");
        }
        for (uint64_t i = 0; i < u64_prefix_count; i++) {
            prefix_t prefix;
            if (! sh.deserialize_integral(prefix)) {
                return sh.fail("couldn't read prefix value for index i==" + to_string_any(i));
            }
            uint64_t u64_bitmap_size = 0;
            if (! sh.deserialize_integral(u64_bitmap_size)) {
                return sh.fail("couldn't read bitmap size for prefix==" + to_string_any(prefix));
            }
            std::vector<char> buffer(u64_bitmap_size);
            if (! sh.read_bytes(buffer.data(), static_cast<size_t>(u64_bitmap_size))) {
                return sh.fail("couldn't read bitmap data for prefix==" + to_string_any(prefix));
            }
            // Try to place the prefix, which will build out bitmap too.
            auto [it, inserted] = _flat_map.try_emplace(prefix);
            if (inserted) {
                add_prefix_key(prefix);
            }
            it->second = roaring::Roaring::read(buffer.data(), true);
        }

        // All good
        return true;
    }



    /**
    * @brief A for-each wrapper returning each value in the bitmap to `callback`.
    *
    * This method uses `for_each_transformer` when TLS storage isn't needed.  It applies `callback` to all values.
    *
    * Callback must have this signature: `(const T& value)`
    *
    * The const and ref prevent GMP allocations when `T` is an `mpz_class`.  Whether caller actually reuses an object is up to
    * them, but this method at least tries to avoid alloc() storms when reconstituting values for processing and passing them back.
    *
    * @warning Caller may NOT modify this bitmap while iterating!  It relies on CRoaring's iterators and internal structures, which
    * are invalidated upon changes.
    *
    * @tparam Func A function signature defined to match `callback`.
    * @param policy The desired policy (currently either Serial or Parallel) for processing.  See node_bitmap_traits.hpp.
    * @param callback Method to invoke on each value.
    */
    template<typename Func>
    void for_each_value(BitmapTransformerPolicy policy, Func&& callback) {
        // Do not allow non-ref callbacks.  Otherwise we make GMP over and over.
        static_assert(std::is_invocable_v<Func, const T&>, "Callback must be callable with (const T&)");
        std::vector<uint8_t> dummy_tls;
        for_each_transformer(policy, dummy_tls, [&](const T& value, auto&) {
            return callback(value);
        });
    }



    /**
    * @brief A for-each transformer allowing callbacks with thread-local storage for transformation.
    *
    * Applies `callback` to all values according to the BitmapTransformerPolicy (serial or parallel) requested.  When serial, order
    * is guaranteed.
    *
    * Callback must have this signature: `(const T& value, TLS_Type& tls)`
    *
    * The const and ref prevent GMP allocations when `T` is an `mpz_class`.  Whether caller actually reuses an object is up to
    * them, but this method at least tries to avoid alloc() storms when reconstituting values for processing and passing them back.
    *
    * @warning Caller may NOT modify this bitmap while iterating!  It relies on CRoaring's iterators and internal structures, which
    * are invalidated upon changes.
    *
    * @tparam Func A function signature defined to match `callback`.
    * @tparam TLS_Type User-selected data type for the vector of thread-local storage to utilize.
    * @param policy The desired policy (currently either Serial or Parallel) for processing.  See node_bitmap_traits.hpp.
    * @param tls A vector to store thread-local data in during callbacks.  This method WILL call `tls.resize()` if the number of
    * available threads reported by `omp_get_max_threads()` exceeds `tls.capacity()`.  From there, each thread is given a slice
    * (indexed element) of that vector.  This storage may be modified at-will in caller's `callback`.
    * @param callback Method to invoke on each value.
    */
    template<typename Func, typename TLS_Type>
    void for_each_transformer(BitmapTransformerPolicy policy, std::vector<TLS_Type>& tls, Func&& callback) {
        // Do not allow non-ref callbacks.  Otherwise we make GMP over and over.
        static_assert(std::is_invocable_r_v<bool, Func, const T&, TLS_Type&>, "Callback must be callable as void(const T&, TLS_Type&)");

        // Ensure the TLS has enough elements before proceeding.
        const size_t max_threads = policy == BitmapTransformerPolicy::SERIAL ? 1 : static_cast<size_t>(omp_get_max_threads());
        if (tls.size() < max_threads) {
            tls.resize(max_threads);
        }

        // Setup a control-stop.
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

            // Even though this path is not threaded, get a thread ID for clarity.
            size_t my_thread_id = static_cast<size_t>(omp_get_thread_num());

            // The TLS should only need index 0.
            TLS_Type& my_tls = tls[my_thread_id];

            // Loop through prefixes in order.
            for (const prefix_t& prefix : _sorted_prefixes) {
                // Shift the prefix once instead of deeper inside the while() loop.  For smaller types, set to 0.
                if constexpr(sizeof(T) > Traits::SUFFIX_BYTES) {
                    // T can hold the prefix.  Let it shift.
                    if constexpr(FixedWidthIntegral<T>) {
                        shifted_prefix = static_cast<T>(prefix) << Traits::SUFFIX_BITS;
                    } else if constexpr(GMPIntegral<T>) {
                        mpz_mul_2exp(shifted_prefix.get_mpz_t(), prefix.get_mpz_t(), Traits::SUFFIX_BITS);
                    }
                } else {
                    // T is smaller than suffix_t, which means the only prefix it'll ever have is 0.
                    // Set it manually, because small-T means it can't shift SUFFIX_BITS wide.
                    shifted_prefix = 0;
                }

                // Find the bitmap and connect an iterator.
                roaring::Roaring& roaring_obj = _flat_map.find(prefix)->second;
                roaring::api::roaring_uint32_iterator_t bitmap_iterator;
                roaring::api::roaring_iterator_init(&roaring_obj.roaring, &bitmap_iterator);

                // Loop this iterator until done.  Use addition instead of bitwise-OR (|) to avoid alloc on GMP path.
                while (bitmap_iterator.has_value) {
                    suffix = bitmap_iterator.current_value;
                    if constexpr(FixedWidthIntegral<T>) {
                        value = shifted_prefix + suffix;
                    } else if constexpr(GMPIntegral<T>) {
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
            // As such, this will read the internal high-low containers and iterate based on their type (array, bitmap, RLE).

            // Open the parallel region early and establish thread-locals.
            #pragma omp parallel default(none) shared(tls, atomic_stop, callback)
            {
                // Get a thread ID.
                size_t my_thread_id = static_cast<size_t>(omp_get_thread_num());

                // Pick an element from the TLS.
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

                    // Grab the prefix and then build a shifted_prefix so it isn't recomputed.
                    const prefix_t& prefix = _sorted_prefixes[prefix_index];
                    // Shift the prefix once instead of deeper inside the while()/for() loops.  For smaller types, set to 0.
                    if constexpr(sizeof(T) > Traits::SUFFIX_BYTES) {
                        // T can hold the prefix.  Let it shift.
                        if constexpr(FixedWidthIntegral<T>) {
                            shifted_prefix = static_cast<T>(prefix) << Traits::SUFFIX_BITS;
                        } else if constexpr(GMPIntegral<T>) {
                            mpz_mul_2exp(shifted_prefix.get_mpz_t(), prefix.get_mpz_t(), Traits::SUFFIX_BITS);
                        }
                    } else {
                        // T is smaller than suffix_t, which means the only prefix it'll ever have is 0.
                        // Set it manually, because small-T means it can't shift SUFFIX_BITS wide.
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

                        // Build the shifted prefix and key combo so it isn't repeated below.
                        if constexpr(FixedWidthIntegral<T>) {
                            shifted_prefix_and_key = shifted_prefix + shifted_key;
                        } else if constexpr(GMPIntegral<T>) {
                            mpz_add_ui(shifted_prefix_and_key.get_mpz_t(), shifted_prefix.get_mpz_t(), shifted_key);
                        }

                        // Select the correct iteration method based on the typecode to retrieve and process suffixes.
                        // Use addition instead of bitwise-OR (|) to avoid alloc on GMP path.
                        switch (typecode) {
                            case ARRAY_CONTAINER_TYPE: {
                                const roaring::internal::array_container_t* array_container = (roaring::internal::array_container_t*)container;
                                for (int array_index = 0; array_index < array_container->cardinality; array_index++) {
                                    suffix_val = array_container->array[array_index];
                                    if constexpr(FixedWidthIntegral<T>) {
                                        value = shifted_prefix_and_key + suffix_val;
                                    } else if constexpr(GMPIntegral<T>) {
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
                                        if constexpr(FixedWidthIntegral<T>) {
                                            value = shifted_prefix_and_key + suffix_val;
                                        } else if constexpr(GMPIntegral<T>) {
                                            mpz_add_ui(value.get_mpz_t(), shifted_prefix_and_key.get_mpz_t(), suffix_val);
                                        }
                                        local_stop = callback(value, my_tls);
                                        if (local_stop) {
                                            atomic_stop.store(true, std::memory_order_relaxed);
                                            break;
                                        }
                                        // Now bitwise-AND the word minus one to wipe out the least-significant 1s place which was just processed.
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
                                        if constexpr(FixedWidthIntegral<T>) {
                                            value = shifted_prefix_and_key + suffix_val;
                                        } else if constexpr(GMPIntegral<T>) {
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
