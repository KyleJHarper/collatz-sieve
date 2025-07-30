#ifndef SRC_BINARY_TREE_COVERAGE_H_
#define SRC_BINARY_TREE_COVERAGE_H_


#include <gmp.h>
#include <gmpxx.h>


//
// Tree Coverage Class
//
class BinaryTreeCoverage {
    private:
    size_t _covered = 0;
    size_t _total = 0;

    public:
    // Constructors
    BinaryTreeCoverage() {}
    BinaryTreeCoverage(size_t covered, size_t total) {
        _covered = covered;
        _total = total;
    }

    // Setters
    void set_covered(size_t covered) {
        _covered = covered;
    }
    void set_total(size_t total) {
        _total = total;
    }
    void add_covered(size_t count=1) {
        _covered += count;
    }
    void add_total(size_t count=1) {
        _total += count;
    }
    // Getters
    const size_t& get_covered() const {
        return _covered;
    }
    const size_t& get_total() const {
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

#endif
