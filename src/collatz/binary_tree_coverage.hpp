#pragma once

#include "concepts.hpp"
#include "binary_tree_math.hpp"
#include <gmp.h>
#include <gmpxx.h>
#include <array>
#include <string>
#include "stream_helpers.hpp"



//
// Keep a record of known coverages.
//
namespace BinaryTreeCoverageConstants {
    // Precomputed coverage for given levels.  Level-indexed (aka element 0 means nothing).
    constexpr size_t MAX_KNOWN_COVERAGE_LEVEL = 45;
    constexpr std::array<uint64_t, MAX_KNOWN_COVERAGE_LEVEL+1> KNOWN_COVERAGE_64BIT = {
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
        , 17508784644300 // 45
    };
    constexpr std::array<uint128_t, MAX_KNOWN_COVERAGE_LEVEL+1> KNOWN_COVERAGE_128BIT = {
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
        , 17508784644300 // 45
    };

    //
    // Get Known Coverage
    // Returns the precomputed coverage from previous testing.
    template<AnySupportedIntegral T>
    inline T get_known_coverage(size_t level) {
        if (level > MAX_KNOWN_COVERAGE_LEVEL) {
            throw std::out_of_range("Level " + std::to_string(level) + " not found in KNOWN_COVERAGE");
        }
        if constexpr(NativeIntegral<T>) {
            return KNOWN_COVERAGE_64BIT[level];
        } else if constexpr(ExtendedIntegral<T>) {
            return KNOWN_COVERAGE_128BIT[level];
        } else if constexpr(GMPIntegral<T>) {
            return uint128_to_mpz(KNOWN_COVERAGE_128BIT[level]);
        }
        throw std::logic_error("Unknown type get_known_coverage()");
    }

    //
    // Get Known Coverage (Sum To Level)
    // Returns the sum of all coverages from level 1 to max_level specified.
    template<AnySupportedIntegral T>
    inline T get_known_coverage_sum_to_level(size_t max_level) {
        T total = 0;
        for (size_t level = 0; level <= max_level; level++) {
            total += get_known_coverage<T>(level);
        }
        return total;
    }

    //
    // Get Total
    // Returns the total number of nodes for a level.  Uses BinaryTreeMath under the hood.
    template<AnySupportedIntegral T>
    inline T get_total(size_t level) {
        return BinaryTreeMath<T>::st_node_count_of_level(level);
    }

    //
    // Get Total (Sum of Levels)
    // Returns the number of nodes between min_level and max_level, inclusive.  Uses BinaryTreeMath under the hood.
    template<AnySupportedIntegral T>
    inline T get_total_sum_of_levels(size_t min_level, size_t max_level) {
        return BinaryTreeMath<T>::st_node_count_of_levels(min_level, max_level);
    }
}


//
// Tree Coverage Class
//
template<AnySupportedIntegral T>
class BinaryTreeCoverage {
    private:
    T _covered = 0;
    T _total = 0;


    public:
    //
    // Constructors
    //
    BinaryTreeCoverage() {
        _covered = 0;
        _total = 0;
    }
    BinaryTreeCoverage(T covered, T total) {
        _covered = covered;
        _total = total;
    }



    //
    // Merge
    // Merge results from another coverage into ours.
    //
    void merge(const BinaryTreeCoverage& other_coverage) {
        _covered += other_coverage.get_covered();
        _total += other_coverage.get_total();
    }



    //
    // Setters
    //
    void set_covered(T covered) { _covered = covered; }
    void set_total(T total) { _total = total; }
    void add_covered(T count=1) { _covered += count; }
    void add_total(T count=1) { _total += count; }



    //
    // Getters
    //
    const T& get_covered() const { return _covered; }
    const T& get_total() const { return _total; }
    T get_uncovered() const { return _total - _covered; }
    //
    // Get the ratio.  If total is 0 or negative, we will throw an error.
    const mpf_class get_ratio(bool as_percent = false) {
        if (_total < 1) {
            throw std::runtime_error("You cannot call get_ratio() when 'total' is 0 or less.");
        }
        // Set r first so it's converted to a float.  Otherwise int/int ==> truncated int.
        mpf_class ratio;
        if constexpr(ExtendedIntegral<T>) {
            ratio = uint128_to_mpf(_covered);
            ratio = ratio / uint128_to_mpz(_total);
        } else {
           ratio = _covered;
           ratio /= _total;
        }
        if(as_percent) {
            ratio *= 100;
        }
        return ratio;
    }



    //
    // Serialize
    //
    [[nodiscard]] bool serialize(std::ostream& out, std::string* err = nullptr) const {
        StreamHelper sh(nullptr, &out, err);
        sh.set_category("BinaryTreeCoverage");

        if (! sh.serialize_integral(_covered)) {
            return sh.fail("_covered==" + to_string_any(_covered));
        }
        if (! sh.serialize_integral(_total)) {
            return sh.fail("_total==" + to_string_any(_total));
        }

        // All Good
        return true;
    }



    //
    // Deserialize
    //
    [[nodiscard]] bool deserialize(std::istream& in, std::string* err) {
        StreamHelper sh(&in, nullptr, err);
        sh.set_category("BinaryTreeCoverage");

        if (! sh.deserialize_integral(_covered)) {
            return sh.fail("couldn't read _covered");
        }
        if (! sh.deserialize_integral(_total)) {
            return sh.fail("couldn't read _total");
        }

        // All good
        return true;
    }

};

