#pragma once

#include "concepts.hpp"
#include <gmp.h>
#include <gmpxx.h>


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
    void set_covered(T covered) {
        _covered = covered;
    }
    void set_total(T total) {
        _total = total;
    }
    void add_covered(T count=1) {
        _covered += count;
    }
    void add_total(T count=1) {
        _total += count;
    }
    // Getters
    const T& get_covered() const {
        return _covered;
    }
    const T& get_total() const {
        return _total;
    }
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
