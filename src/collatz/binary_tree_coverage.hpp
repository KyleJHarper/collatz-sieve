#pragma once

#include "concepts.hpp"
#include <gmp.h>
#include <gmpxx.h>
#include <array>


//
// Keep a record of known coverages.
//
namespace BinaryTreeCoverageConstants {
    // Precomputed coverage for given levels.
    constexpr size_t MAX_KNOWN_COVERAGE_LEVEL = 32;
    constexpr std::array<size_t, MAX_KNOWN_COVERAGE_LEVEL+1> KNOWN_COVERAGE = {
        0,  // 0
        1,  // 1
        3,  // 2
        6,  // 3
        13,  // 4
        28,  // 5
        56,  // 6
        115,  // 7
        237,  // 8
        474,  // 9
        960,  // 10
        1920,  // 11
        3870,  // 12
        7825,  // 13
        15650,  // 14
        31473,  // 15
        63422,  // 16
        126844, // 17
        254649,  //18
        509298,  // 19
        1021248,  // 20
        2050541,  // 21
        4101082,  // 22
        8219801,  // 23
        16490635,  // 24
        32981270,  // 25
        66071490,  // 26
        132455435,  // 27
        264910870,  // 28
        530485275,  // 29
        1060970550,  // 30
        2123841570,  // 31
        4253619813,  // 32
    };
    inline size_t get_known_coverage(size_t level) {
        if (level > MAX_KNOWN_COVERAGE_LEVEL) {
            throw std::out_of_range("Level " + std::to_string(level) + " not found in KNOWN_COVERAGE");
        }
        return KNOWN_COVERAGE[level];
    }
    inline size_t get_known_coverage_sum_to_level(size_t max_level) {
        size_t total = 0;
        for (size_t level = 0; level <= max_level; level++) {
            total += get_known_coverage(level);
        }
        return total;
    }
    inline size_t get_total(size_t level) {
        return size_t(1) << level;
    }
    inline size_t get_total_sum_to_level(size_t min_level, size_t max_level) {
        size_t total = 0;
        for (size_t level = min_level; level <= max_level; level++) {
            total += get_total(level);
        }
        return total;
    }
}


//
// Tree Coverage Class
//
template<IntegralOrMPZClass T>
class BinaryTreeCoverage {
    private:
    T _covered = 0;
    T _total = 0;

    public:
    // Constructors
    BinaryTreeCoverage() {
        _covered = 0;
        _total = 0;
    }
    BinaryTreeCoverage(T covered, T total) {
        _covered = covered;
        _total = total;
    }

    // Merge results from another coverage into ours.
    void merge(const BinaryTreeCoverage& other_coverage) {
        _covered += other_coverage.get_covered();
        _total += other_coverage.get_total();
    }
    // Setters
    void set_covered(T covered) { _covered = covered; }
    void set_total(T total) { _total = total; }
    void add_covered(T count=1) { _covered += count; }
    void add_total(T count=1) { _total += count; }
    // Getters
    const T& get_covered() const { return _covered; }
    const T& get_total() const { return _total; }
    T get_uncovered() const { return _total - _covered; }
    // Get the ratio.  If total is 0 or negative, we will throw an error.
    const mpf_class get_ratio(bool as_percent = false) {
        if (_total < 1) {
            throw std::runtime_error("You cannot call get_ratio() when 'total' is 0 or less.");
        }
        // Set r first so it's converted to a float.  Otherwise int/int ==> truncated int.
        mpf_class ratio = _covered;
        ratio /= _total;
        if(as_percent) {
            ratio *= 100;
        }
        return ratio;
    }

};
