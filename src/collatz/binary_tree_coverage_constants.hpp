#pragma once

#include "abi.hpp"
#include "concepts.hpp"
#include "binary_tree_math.hpp"
#include <array>
#include <stdexcept>


/**
* @namespace BinaryTreeCoverageConstants
* @brief Storage for known (precomputed) coverage of tree levels, and a few helpers for summing them.
*/
namespace BinaryTreeCoverageConstants {

    /// @brief Maximum known (computed) level of coverage.
    constexpr level_t MAX_KNOWN_COVERAGE_LEVEL = 50;



    /// @brief Map of precomputed coverage up to `MAX_KNOWN_COVERAGE_LEVEL`.
    /// @note Uses 128-bit fixed for now.  If this is exceeded in the future, it can be switched to a static mpz_class array.
    constexpr std::array<uint128_t, MAX_KNOWN_COVERAGE_LEVEL+1> KNOWN_COVERAGE_MAP = {
        0  // 0
        , 0  // 1
        , 1  // 2
        , 3  // 3
        , 6  // 4
        , 13  // 5
        , 28  // 6
        , 56  // 7
        , 115  // 8
        , 237  // 9
        , 474  // 10
        , 960  // 11
        , 1920  // 12
        , 3870  // 13
        , 7825  // 14
        , 15650  // 15
        , 31473  // 16
        , 63422  // 17
        , 126844 // 18
        , 254649  //19
        , 509298  // 20
        , 1021248  // 21
        , 2050541  // 22
        , 4101082  // 23
        , 8219801  // 24
        , 16490635  // 25
        , 32981270  // 26
        , 66071490  // 27
        , 132455435  // 28
        , 264910870  // 29
        , 530485275  // 30
        , 1060970550  // 31
        , 2123841570  // 32
        , 4253619813  // 33
        , 8507239626  // 34
        , 17027951548  // 35
        , 34095896991  // 36
        , 68191793982  // 37
        , 136471574881  // 38
        , 272943149762  // 39
        , 546144278026  // 40
        , 1093108792776  // 41
        , 2186217585552  // 42
        , 4374334645782  // 43
        , 8754392322150  // 44
        , 17508784644300  // 45
        , 35030378766136  // 46
        , 70098794380682  // 47
        , 140197588761364  // 48
        , 280479319327805  // 49
        , 560958638655610  // 50
    };



    /**
    * @brief Get known coverage for any given level.
    *
    * Looks up the precomputed coverage from `KNOWN_COVERAGE_MAP` and casts the return to caller's type `T`.
    *
    * @tparam T Any supported integral (see concepts.hpp).
    * @param level The level of coverage requested.
    * @return The number of nodes covered on `level`, typed to your `T`.
    */
    template<AnySupportedIntegral T>
    inline T get_known_coverage(level_t level) {
        // Sanity checks.
        if (level > MAX_KNOWN_COVERAGE_LEVEL) {
            throw std::out_of_range("Level " + std::to_string(level) + " not found in KNOWN_COVERAGE");
        }

        // Pick the right type, and convert as-needed.
        if constexpr(FixedWidthIntegral<T>) {
            if (level > sizeof(T) * 8) {
                throw std::out_of_range("Level " + std::to_string(level) + " is outside the max range for your type T: " + ABI::demangle<T>());
            }
            return static_cast<T>(KNOWN_COVERAGE_MAP[level]);
        } else if constexpr(GMPIntegral<T>) {
            return Int128::uint128_to_mpz(KNOWN_COVERAGE_MAP[level]);
        } else {
            throw std::logic_error("Unknown data type");
        }

        // Should never reach this.
        throw std::logic_error("Unknown type get_known_coverage()");
    }



    /**
    * @brief Get known coverage of a whole tree, not just a single level.
    *
    * Tallies the sum of all coverages from level 1 to max_level specified, inclusive.
    *
    * @tparam T Any supported integral (see concepts.hpp).
    * @param max_level Maximum level to tally, inclusive.
    * @return The number of nodes covered on all levels up to `max_level`, typed to your `T`.
    */
    template<AnySupportedIntegral T>
    inline T get_known_coverage_sum_to_level(level_t max_level) {
        T total = 0;
        for (level_t level = 0; level <= max_level; level++) {
            total += get_known_coverage<T>(level);
        }
        return total;
    }



    /**
    * @brief Get node total for a level.
    *
    * Simple helper when dealing with coverage which uses `BinaryTreeMath` under the hood.
    *
    * @tparam T Any supported integral (see concepts.hpp).
    * @param level The level to get total node count for.
    * @return Number of nodes on `level`.
    */
    template<AnySupportedIntegral T>
    inline T get_total(level_t level) {
        return BinaryTreeMath<T>::st_node_count_of_level(level);
    }



    /**
    * @brief Get node total for multiple levels.  Fully closed.
    *
    * Simple helper when dealing with coverage which uses `BinaryTreeMath` under the hood.
    *
    * @tparam T Any supported integral (see concepts.hpp).
    * @param min_level Minimum level to start tallying, inclusive.
    * @param max_level Maximum level to start tallying, inclusive.
    * @return Number of nodes on fully-closed `[min_level, max_level]`.
    */
    template<AnySupportedIntegral T>
    inline T get_total_sum_of_levels(level_t min_level, level_t max_level) {
        return BinaryTreeMath<T>::st_node_count_of_levels(min_level, max_level);
    }

}
