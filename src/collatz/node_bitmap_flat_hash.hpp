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
    // Add Range
    // Add a contiguous range of values using the most optimized approach.  Only works well when the prefixes match.
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
            auto& first = _flat_map.try_emplace(start_prefix).first->second;
            first.addRange(start_suffix, Traits::SUFFIX_MAX);

            // Loop through any intermediary prefixes, which are incremental of course between prefix_start and prefix_end, non-inclusive.
            for (prefix_t middle_prefix = start_prefix + 1; middle_prefix < end_prefix; middle_prefix++) {
                auto& mid = _flat_map.try_emplace(middle_prefix).first->second;
                mid.addRange(0, Traits::SUFFIX_MAX);
            }

            // Add the remaining bits to the last prefix, ending at "end" of course.
            auto& last = _flat_map.try_emplace(end_prefix).first->second;
            last.addRange(0, end_prefix);
        }
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

};
