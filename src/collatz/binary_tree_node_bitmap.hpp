#pragma once

#include "concepts.hpp"
#include <roaring/roaring.hh>
#include <gmp.h>
#include <absl/container/flat_hash_map.h>
#include <gmpxx.h>




template<AnySupportedIntegral T>
class NodeBitmap {
    private:
    // Prefix type depends on integer width.
    using prefix_t =
        std::conditional_t<
            std::is_same_v<T, mpz_class>
                , mpz_class
                , std::conditional_t<(sizeof(T) <= 4)
                    , uint32_t
                    , std::conditional_t<(sizeof(T) <= 8)
                        , uint32_t
                        , std::conditional_t<(sizeof(T) <= 16)
                            , uint64_t
                            , void
                        >
                    >
                >
        >;
    // Reserve the lower 32 bits for suffix (roaring) for their recommended 16 + 16 internal keying+containers.
    using suffix_t = uint32_t;
    static constexpr suffix_t SUFFIX_BITS = 32;
    static constexpr suffix_t SUFFIX_MASK = std::numeric_limits<suffix_t>::max();
    // Use Abseil to build the map.
    absl::flat_hash_map<prefix_t, roaring::Roaring> _buckets;



    //
    // Prefix
    // Get the prefix from any value.
    //
    static prefix_t prefix(const T& value) {
        if constexpr(BuiltinIntegral<T>) {
            if constexpr (sizeof(T) <= 4) {
                return 0;
            } else {
                return (prefix_t)(value >> SUFFIX_BITS);
            }
        } else {
            // Use a thread local to reduce alloc() churn.
            static thread_local mpz_class rv;
            mpz_fdiv_q_2exp(rv.get_mpz_t(), value.get_mpz_t(), SUFFIX_BITS);
            return rv;
        }
    }



    //
    // Suffix
    // Get the suffix from any value.
    //
    static suffix_t suffix(const T& value) {
        if constexpr(BuiltinIntegral<T>) {
            return (suffix_t)(value & SUFFIX_MASK);
        } else {
            // Even though this is a GMP type, we are returning a UI type from it, which should be cheap.
            return (suffix_t)(value.get_ui());
        }
    }



    public:
    //
    // Default Constructor
    //
    NodeBitmap() {}



    //
    // Add
    // Add a value to the bitmap.
    //
    void add(const T& value) {
        auto p = prefix(value);
        auto s = suffix(value);

        auto [it, inserted] = _buckets.try_emplace(p);
        it->second.add(s);
    }



    //
    // Contains
    // Check for a value to exist in the bitmap.
    //
    bool contains(const T& value) const {
        auto p = prefix(value);
        auto s = suffix(value);

        auto it = _buckets.find(p);
        if (it == _buckets.end()) {
            return false;
        }

        return it->second.contains(s);
    }



    //
    // Add Range
    // Add a contiguous range of values using the most optimized approach.  Only works well when the prefixes match.
    //
    void add_range(const T& start, const T& end) {
        auto p1 = prefix(start);
        auto p2 = prefix(end);
        if (p1 != p2) {
            // Have to do it manually.  Rip.
            for (T v = start; v < end; v++) {
                add(v);
            }
            return;
        }

        // Prefixes match.  Call the map's native addRange().
        auto [it, inserted] = _buckets.try_emplace(p1);
        it->second.addRange(suffix(start), suffix(end));
    }



    //
    // Optimize
    // Calls the internal runOptimize() for each bucket, which (I think) is an RLE analysis and/or compaction task.
    //
    void optimize() {
        for (auto& [key ,bucket] : _buckets) {
            bucket.runOptimize();
        }
    }
};