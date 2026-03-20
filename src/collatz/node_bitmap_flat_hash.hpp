#pragma once

#include "concepts.hpp"
#include "node_bitmap_concepts_and_traits.hpp"
#include <absl/container/flat_hash_map.h>
#include <roaring/roaring.hh>



//
// FlatHashBitmap
// An implementation of the roaring bitmap for our NodeBitmap, using an Asbeil flat hashmap for O(1) performance on native types up
// to 128-bit.  Does not work with mpz_class.
//
template<AnySupportedIntegral T>
class FlatHashBitmapImpl {
    private:
    using Traits = BitmapKeyTraits<T>;
    using prefix_t = Traits::prefix_t;
    using suffix_t = Traits::suffix_t;
    absl::flat_hash_map<prefix_t, roaring::Roaring> _flat_map;



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
        prefix_t prefix_1 = Traits::get_prefix(start);
        prefix_t prefix_2 = Traits::get_prefix(end);
        if (prefix_1 != prefix_2) {
            // Have to do it manually.  Rip.
            for (T value = start; value < end; value++) {
                add(value);
            }
            return;
        }

        // Prefixes match.  Call the map's native addRange().
        auto [it, inserted] = _flat_map.try_emplace(prefix_1);
        it->second.addRange(Traits::get_suffix(start), Traits::get_suffix(end));
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
