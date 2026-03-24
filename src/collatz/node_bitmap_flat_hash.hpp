#pragma once

#include "concepts.hpp"
#include "node_bitmap_traits.hpp"
#include <absl/container/flat_hash_map.h>
#include <roaring/roaring.hh>



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
    using map_t = std::conditional_t<
        BuiltinIntegral<T>
        , absl::flat_hash_map<prefix_t, roaring::Roaring>
        , absl::flat_hash_map<prefix_t, roaring::Roaring, absl::Hash<prefix_t>, MpzEq>
    >;
    map_t _flat_map;



    public:
    FlatHashBitmapImpl() = default;



    //
    // Add
    // Add a value to the bitmap.
    //
    void add(const T& value) {
        prefix_t prefix = Traits::get_prefix(value);
        suffix_t suffix = Traits::get_suffix(value);

        auto [it, inserted] = _flat_map.try_emplace(prefix);
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
            it->second.addRange(start_suffix, end_suffix);
        } else {
            // Insert the remainder of prefix_1 which clearly goes to SUFFIX_MAX since prefix_2 is beyond it.
            // Since the suffix is full, we use addRangeClosed() to encapsulate it.
            auto& first = _flat_map.try_emplace(start_prefix).first->second;
            first.addRangeClosed(start_suffix, Traits::SUFFIX_MAX);

            // Loop through any intermediary prefixes, which are incremental of course between prefix_start and prefix_end, non-inclusive.
            // Again, since the suffix is guaranteed full, we use addRangeClosed() to encapsulate it.
            for (prefix_t middle_prefix = start_prefix + 1; middle_prefix < end_prefix; middle_prefix++) {
                auto& mid = _flat_map.try_emplace(middle_prefix).first->second;
                mid.addRangeClosed(0, Traits::SUFFIX_MAX);
            }

            // Add the remaining bits to the last prefix, ending at "end" of course.
            // Reminder, we do NOT include the last value (end_suffix) because this is the half-open variant.
            auto& last = _flat_map.try_emplace(end_prefix).first->second;
            last.addRange(0, end_suffix);
        }
    }
    //
    // Now we add the CLOSED range (max is included).
    //
    void add_range_closed(const T& start, const T& end) {
        prefix_t start_prefix = Traits::get_prefix(start);
        suffix_t start_suffix = Traits::get_suffix(start);
        prefix_t end_prefix = Traits::get_prefix(end);
        suffix_t end_suffix = Traits::get_suffix(end);
        if (start_prefix == end_prefix) {
            // They all fall into the same prefix.  Call a single addRange() and then leave.
            auto [it, inserted] = _flat_map.try_emplace(start_prefix);
            it->second.addRangeClosed(start_suffix, end_suffix);
        } else {
            // Insert the remainder of prefix_1 which clearly goes to SUFFIX_MAX since prefix_2 is beyond it.
            auto& first = _flat_map.try_emplace(start_prefix).first->second;
            first.addRangeClosed(start_suffix, Traits::SUFFIX_MAX);

            // Loop through any intermediary prefixes, which are incremental of course between prefix_start and prefix_end, non-inclusive.
            for (prefix_t middle_prefix = start_prefix + 1; middle_prefix < end_prefix; middle_prefix++) {
                auto& mid = _flat_map.try_emplace(middle_prefix).first->second;
                mid.addRangeClosed(0, Traits::SUFFIX_MAX);
            }

            // Add the remaining bits to the last prefix, ending at "end" of course.
            auto& last = _flat_map.try_emplace(end_prefix).first->second;
            last.addRangeClosed(0, end_suffix);
        }
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
        }
    }



    //
    // Clear
    // Removes all items from all bitmaps by clearing the entire hash object.
    //
    void clear() {
        _flat_map.clear();
    }



    //
    // Optimize
    // Calls the internal runOptimize() for each roaring map, which (I think) is an RLE analysis and/or compaction task.
    //
    void optimize() {
        for (auto& [key, bucket] : _flat_map) {
            bucket.runOptimize();
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

};
