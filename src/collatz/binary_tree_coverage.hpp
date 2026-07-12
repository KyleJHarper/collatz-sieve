#pragma once

#include "concepts.hpp"
#include "string.hpp"
#include <gmp.h>
#include <gmpxx.h>
#include <stdexcept>
#include <string>
#include "stream_helper.hpp"
#include "equality_helper.hpp"




/**
* @class BinaryTreeCoverage
* @brief Container to track the number of nodes covered (eliminated) on a level in a `BinaryTree`.
* @tparam T Any supported integral (see concepts.hpp).
*/
template<AnySupportedIntegral T>
class BinaryTreeCoverage {
    private:
    /// @brief The number of nodes covered.
    T _covered = 0;
    /// @brief The number of total nodes.
    T _total = 0;



    public:
    /// @name Lifecycle Management
    /// @{

    /**
    * @brief Default constructor.
    */
    BinaryTreeCoverage() = default;



    /**
    * @brief Constructor which sets covered and total initially.
    * @param covered The number of covered to set.
    * @param total The number of total to set.
    */
    BinaryTreeCoverage(T covered, T total) {
        _covered = covered;
        _total = total;
    }



    /**
    * @brief Default destructor.
    */
    ~BinaryTreeCoverage() = default;

    /// @}



    /// @name Accessors
    /// @{

    /// @brief Set the number of covered directly.
    void set_covered(T covered) { _covered = covered; }
    /// @brief Set the total directly.
    void set_total(T total) { _total = total; }
    /// @brief Add `count` to the covered.
    void add_covered(T count=1) { _covered += count; }
    /// @brief Add `count` to the total.
    void add_total(T count=1) { _total += count; }
    /// @brief Get covered tally.
    const T& get_covered() const { return _covered; }
    /// @brief Get total tally.
    const T& get_total() const { return _total; }
    /// @brief Get uncovered tally, which is `_total - _covered`.
    /// @note The subtraction and return cause an instantiation.  For `mpz_class`, that's an alloc.
    T get_uncovered() const { return _total - _covered; }

    /// @}



    /**
    * @brief Get ratio of covered / total.
    * @param as_percent Multiplies the result by 100 such that 0.972 becomes 97.2.
    * @return The ratio as an `mpf_class`.
    */
    mpf_class get_ratio(bool as_percent = false) const {
        if (_total < 1) {
            throw std::out_of_range("You cannot call get_ratio() when 'total' is 0 or less.");
        }
        // Set r first so it's converted to a float.  Otherwise int/int ==> truncated int.
        mpf_class ratio;
        if constexpr(Int128Integral<T>) {
            ratio = Int128::uint128_to_mpf(_covered);
            ratio = ratio / Int128::uint128_to_mpz(_total);
        } else {
           ratio = _covered;
           ratio /= _total;
        }
        if(as_percent) {
            ratio *= 100;
        }
        return ratio;
    }



    /**
    * @brief Merge results from another coverage into this one.
    * @param other_coverage Another instance of `BinaryTreeCoverage` to merge into `this`.
    */
    void merge(const BinaryTreeCoverage& other_coverage) {
        _covered += other_coverage.get_covered();
        _total += other_coverage.get_total();
    }



    /**
    * @brief Serialize this object for export.
    *
    * Serialization happens in this order:
    *   1. Covered (integral)
    *   2. Total (integral)
    *
    * @note This method does not throw.
    * @param out The stream to write data to.
    * @param err Pointer to a string where errors, if any, are written.
    * @return A boolean indicating success or failure.  Do not discard.
    */
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



    //
    // Equal
    // Compares this coverage to "other".  Returns true if equal.
    //
    // Returns true if they are equal in representation.  False otherwise.
    // Will explain what failed to *err if sent.
    //
    /**
    * @brief Compare two BinaryTreeCoverages' specific internals and return true if identical.
    *
    * This function checks:
    *   1. Totals match.
    *   2. Covered match.
    *
    * @param first The first BinaryTreeCoverage to compare.
    * @param second The second BinaryTreeCoverage to compare.
    * @param err Pointer to a string where inequality or error messages are stored.
    * @return True if equal, false otherwise.
    */
    static bool st_equal (const BinaryTreeCoverage<T>& first, const BinaryTreeCoverage<T>& second, std::string* err = nullptr) {
        EqualityHelper eq(err);
        eq.set_category("BinaryTreeCoverage");

        // Total
        if (! eq.equal(first.get_total(), second.get_total())) {
            return eq.fail("Coverages' totals don't match");
        }

        // Covered
        if (! eq.equal(first.get_covered(), second.get_covered())) {
            return eq.fail("Coverages' covered values don't match");
        }

        // All good.
        return true;
    }



    /**
    * @brief Compare another coverage to this one.
    *
    * This is a member helper which simply forwards to `BinaryTreeCoverage::st_equal()`.
    *
    * @param second The second BinaryTreeCoverage to compare against this.
    * @param err Pointer to a string where inequality or error messages are stored.
    * @return True if equal, false otherwise.
    */
    bool equal(const BinaryTreeCoverage<T>& second, std::string* err = nullptr) const {
        return BinaryTreeCoverage<T>::st_equal(*this, second, err);
    }

};
