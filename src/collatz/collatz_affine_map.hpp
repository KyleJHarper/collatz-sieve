#pragma once
#include "concepts.hpp"
#include "exponents.hpp"
#include <gmp.h>
#include "collatz_constants.hpp"
#include <stdexcept>
#include <string>



//
// Collatz Affine Maps
//
// An FG chain can be stored and repsented as an affine map with powers of two and three, and with a constant portion.  This avoids
// any realtime tracking of fractional values (floats).
//
// Note, there are overflow concerns:
// The constant_portion grows the most when the chain is all F.  This worst case leaves constant_portion == ~3^k.  We need about
// 1.6 bits to represent this in binary, so we get the following:
//    64 bits ==> 40
//   128 bits ==> 80
//
// To resolve this, Node uses an upgraded T when nessary.  Anyone else using this needs to provide the same protection.
//




//
// CollatzAffineMap
// The basic map which performs all steps.
//
/**
* @class CollatzAffineMap
* @brief An explicit map which tracks 2's and 3's exponents (`A`) and the constant portion (`B`) for affine transformation.
*
* An FG chain can be stored and represented as an affine map with powers of two and three, and with a constant portion.  This
* avoids any realtime tracking of fractional values (floats).
*
* There are overflow concerns, which are most extreme when the chain is all `F` steps.  This worst case leaves `constant_portion`
* equal to `~3^k`.  Each power of three needs ~1.6 bits to represent this in binary, so we have the following limits:
*   * 64-bits) 40
*   * 128-bits) 80
*
* Therefore, when using large maps, callers must be aware of overflow risks.  These 40 and 80 limits are available via the
* `Exponents::POW3_64BIT_ELEMENT_COUNT` and `Exponents::POW3_128BIT_ELEMENT_COUNT`.
*
* @tparam T Any supported integral (see concepts.hpp).
*/
template<AnySupportedIntegral T>
class CollatzAffineMap {
    private:
    /// @brief The power of two accumulated.  Increases by 1 on both `F` and `G` steps.
    uint32_t _twos_exp = 0;
    /// @brief The power of three accumulated.  Increases by 1 on `F` steps.
    uint32_t _threes_exp = 0;
    /// @brief The constant portion (B) accumulated.  It's affected by `F` and `G` steps.
    T _constant_portion = 0;


    public:
    /// @name Lifecycle Management
    /// @{

    /// @brief Default constructor.
    CollatzAffineMap() {}



    /// @brief Resets the object, treating it like it's new.
    void reset() {
        _twos_exp = 0;
        _threes_exp = 0;
        _constant_portion = 0;
    }

    /// @}



    /// @brief Applies the accelerated `F` step by incrementing exponent counts and transforming `constant_portion` too.
    void apply_F() {
        // Adjust the constant portion before we modify any exponents.
        if constexpr(BuiltinIntegral<T>) {
            // Built-in types can bit shift cheaper than looking up a table.  Do that instead.
            _constant_portion = ((_constant_portion << 1) + _constant_portion) + (T(1) << _twos_exp);
        } else {
            // MPZ Class can do an mpz_add from a precaulated item in a table faster than an initialization + shift operator.
            mpz_mul(_constant_portion.get_mpz_t(), _constant_portion.get_mpz_t(), CollatzConstants::MPZ_THREE.get_mpz_t());
            mpz_add(_constant_portion.get_mpz_t(), _constant_portion.get_mpz_t(), Exponents::get_power_of_two<T>(_twos_exp).get_mpz_t());
        }

        // Now bump the exponents.
        _threes_exp += 1;
        _twos_exp += 1;
    }



    /// @brief Applies the accelerated `G` step by incrementing the two's exponent.
    void apply_G() {
        _twos_exp += 1;
    }



    /**
    * @brief Applies the affine transformation on the value provided and returns the result.
    * @param value The value to treat as `x` in the affine formula: `Ax + b`.
    * @returns A copy of the result, which is an alloc on the GMP path.
    */
    T calculate(const T& value) const {
        T result = Exponents::get_power_of_three<T>(_threes_exp);
        if constexpr(BuiltinIntegral<T>) {
            if (__builtin_mul_overflow(result, value, &result) || __builtin_add_overflow(result, _constant_portion, &result)) {
                throw std::overflow_error(
                    "Overflow in CollatzAffineMap calculate() method.  Value is " + to_string_any(value)
                    + "  Threes exponent is " + to_string_any(_threes_exp)
                    + "  Constant portion is " + to_string_any(_constant_portion)
                    + "."
                );
            }
            result >>= _twos_exp;
        } else if constexpr(GMPIntegral<T>) {
            mpz_mul(result.get_mpz_t(), result.get_mpz_t(), value.get_mpz_t());
            mpz_add(result.get_mpz_t(), result.get_mpz_t(), _constant_portion.get_mpz_t());
            mpz_fdiv_q_2exp(result.get_mpz_t(), result.get_mpz_t(), _twos_exp);
        } else {
            throw std::logic_error("Unknown data type for calculate().");
        }
        return result;
    }



    /**
    * @brief Applies the affine transformation on the value provided and stores the result in `out`.
    * @param value The value to treat as `x` in the affine formula: `Ax + b`.
    * @param out The resut stored in callers memory, which avoids alloc on the GMP path.
    */
    void calculate(const T& value, T& out) const {
        if constexpr(BuiltinIntegral<T>) {
            out = Exponents::get_power_of_three<T>(_threes_exp);
            if (__builtin_mul_overflow(out, value, &out) || __builtin_add_overflow(out, _constant_portion, &out)) {
                throw std::overflow_error(
                    "Overflow in CollatzAffineMap calculate() method.  Value is " + to_string_any(value)
                    + "  Threes exponent is " + to_string_any(_threes_exp)
                    + "  Constant portion is " + to_string_any(_constant_portion)
                    + "."
                );
            }
            out >>= _twos_exp;
        } else if constexpr(GMPIntegral<T>) {
            mpz_mul(out.get_mpz_t(), Exponents::get_power_of_three<T>(_threes_exp).get_mpz_t(), value.get_mpz_t());
            mpz_add(out.get_mpz_t(), out.get_mpz_t(), _constant_portion.get_mpz_t());
            mpz_fdiv_q_2exp(out.get_mpz_t(), out.get_mpz_t(), _twos_exp);
        } else {
            throw std::logic_error("Unknown data type for calculate().");
        }
    }


    /**
    * @brief Helper which simply applies `calculate()` and returns if it's below (true) or above (false) the original `value`.
    * @param value The value to calculate and compare to.
    * @returns True if the affine transoformation is overall contractive, false otherwise.
    */
    bool is_below(const T& value) const {
        static thread_local T tmp;
        if constexpr(BuiltinIntegral<T>) {
            return calculate(value) < value;
        } else if constexpr(GMPIntegral<T>) {
            calculate(value, tmp);
            return tmp < value;
        } else {
            throw std::logic_error("Unknown type for is_below()");
        }
    }
};




/**
* @class CollatzAffineMapShortcut
* @brief An accelerated map which only tracks the `A` portion of: `Ax + b`.
*
* The distribution of ℕ over Harper's tree, along with the application and limitation of `F` and `G` steps, create constant
* portions (B) which cannot overcome an associated contractive exponential portion (A) to bring the calculated value above itself.
*
* In other words:
*     * Given f = number of F steps
*     * Given k = number of F and G steps
*     * Given A = 3^f/2^k
*     * When A < 1, then:  A * N + B < N
*
* While possibly asymptotic, this has been empirically validated up to 10,000 serial `F` steps.  For safety, a counter has been
* added to ensure this class throws an exception above that level.
*
* The ability to discard the B-portion means only the contractive nature of `A` matters.  These have been precomputed and stored in
* `Exponents::MAX_POW2_UNDER_POW3`.
*
* @note This class has no calculate method, because the B-portion isn't tracked.
* @note Due to the nature of this class, no type `T` is necessary or possible.
*/
class CollatzAffineMapShortcut {
    private:
    /// @brief The power of two accumulated.  Increases by 1 on both `F` and `G` steps.
    uint32_t _twos_exp = 0;
    /// @brief The power of three accumulated.  Increases by 1 on `F` steps.
    uint32_t _threes_exp = 0;


    public:
    /// @brief The highest level of serial `F` steps empirically tested.  Used for validation in `apply_F()`.
    const static size_t MAX_SERIAL_F_TESTED = 10000;



    /// @name Lifecycle Management
    /// @{

    /// @brief Default constructor.
    CollatzAffineMapShortcut() {}



    /// @brief Resets the object, treating it like it's new.
    void reset() {
        _twos_exp = 0;
        _threes_exp = 0;
    }

    /// @}



    /// @brief Increments counts, and that's all.
    void apply_F() {
        // Test to ensure we aren't in excess of the empirically tested limit for contractive A * N + B <= N.
        // Only applies to F steps, because only they grow B.
        if (_threes_exp >= MAX_SERIAL_F_TESTED) {
            throw std::out_of_range("AffineMapShortcut cannot apply another F step because the maximum of " + std::to_string(MAX_SERIAL_F_TESTED) + " serial F steps has been reached.");
        }
        _threes_exp += 1;
        _twos_exp += 1;
    }



    /// @brief Increments counts, and that's all.
    void apply_G() {
        _twos_exp += 1;
    }



    /**
    * @brief Compares the result of powers of 3 and 2, and returns true if `3^f < 2^k`.
    *
    * These are precomputed and stored in an array within the `Exponents` namespace.
    *
    * @return True if `3^f < 2^k`, otherwise false.
    */
    bool is_below() const {
        // If our two's exponent exceeds the precalculated two's-exponent for our power of three, we're below.
        if (_threes_exp > Exponents::MAX_POW2_UNDER_POW3_COUNT) {
            throw std::out_of_range(
                "Cannot lookup MAX_POW2_UNDER_POW3 with _threes_exp of "
                + std::to_string(_threes_exp)
                + " because the max known power of three under two is "
                + std::to_string(Exponents::MAX_POW2_UNDER_POW3_COUNT)
                + "."
            );
        }
        return _twos_exp > Exponents::MAX_POW2_UNDER_POW3[_threes_exp];
    }
};
