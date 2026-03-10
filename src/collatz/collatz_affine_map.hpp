#pragma once
#include "collatz.hpp"
#include "concepts.hpp"
#include "exponents.hpp"
#include <gmp.h>
#include <stdexcept>
#include <string>



//
// Collatz Affine Maps
//
// An FG chain can be stored and repsented as an affine map with powers of two and three, and with a constant portion.  This avoids
// any realtime tracking of fractional values (floats).
//
// Additionally, by happenstance, we're safe regarding data type.  Here's the breakdown.
// The constant_portion grows the most when the chain is all F.  This worst case leaves constant_portion == ~3^k.  We need about
// 1.6 bits to represent this in binary, so we get the following:
//    64 bits ==> 40
//   128 bits ==> 80
// By chance, BinaryTree already ensures a level will fit when add_level() is called, based on the known (precalculated) inital
// values whose sequences will fit within a bit size.  For uint64_t, this is 33.  For uint128_t, it is <<<TODO>>> (still calculating.)
//




//
// CollatzAffineMap
// The basic map which performs all steps.
//
template<AnySupportedIntegral T>
class CollatzAffineMap {
    private:
    uint32_t _twos_exp = 0;
    uint32_t _threes_exp = 0;
    T _constant_portion = 0;


    public:
    CollatzAffineMap() {}



    //
    // Reset
    // Reset the object for reuse.
    //
    void reset() {
        _twos_exp = 0;
        _threes_exp = 0;
        _constant_portion = 0;
    }



    //
    // Apply F
    // Increments counts and bumps the constant_portion according to the accelerated F(x) ==> (3x + 1) / 2
    //
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



    //
    // Apply G
    // Applies the accelerated G(x) ==> x/2.  This is just a bump on the twos_exp.
    //
    void apply_G() {
        _twos_exp += 1;
    }



    //
    // Calculate
    // Applies the the map to the value specified and returns the result.
    //
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
    //
    // Alternate version with an out param to let caller control allocations.
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


    //
    // Is Below
    // Applies the affine map to the value passed in to determine if will be below (true) or above (false) the initial value.
    //
    bool is_below(const T& value) const {
        // We will use lookup tables to save time.  We will hide an mpz_class to avoid alloc()s for callers quietly.
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




//
// CollatzAffineMapShortcut
// An accelerated map which only tracks exponents of 3 and 2 (for F and G).  Our distribution of N means the constant portion of an
// FG chain (B) cannot overcome a contractive exponential portion (A) to bring the calculated value above N.
//
// In other words:
//     Given f = number of F steps
//     Given k = number of F and G steps
//     Given A = 3^f/2^k
//     When A < 1, then:  A * N + B < N
//
// While possibly asymptotic, this has been empirically validated up to 10,000 serial F's and found to hold true.  For safety,
// we'll add a counter to ensure this class throws an exception above our emprically tested level.
//
// We can therefore track exponents of 3 and 2, and perform a simple integer comparison against a precalculated map for is_below().
//
// Note: This version has no calculate() method, because that would be silly.
//
template<AnySupportedIntegral T>
class CollatzAffineMapShortcut {
    private:
    uint32_t _twos_exp = 0;
    uint32_t _threes_exp = 0;


    public:
    const static size_t MAX_SERIAL_F_TESTED = 10000;
    CollatzAffineMapShortcut() {}



    //
    // Reset
    // Reset the object for reuse.
    //
    void reset() {
        _twos_exp = 0;
        _threes_exp = 0;
    }



    //
    // Apply F
    // Increments counts, and that's all.
    //
    void apply_F() {
        // Test to ensure we aren't in excess of the empirically tested limit for contractive A * N + B <= N.
        // Only applies to F steps, because only they grow B.
        if (_threes_exp >= MAX_SERIAL_F_TESTED) {
            throw std::out_of_range("AffineMapShortcut cannot apply another F step because the maximum of " + std::to_string(MAX_SERIAL_F_TESTED) + " serial F steps has been reached.");
        }
        _threes_exp += 1;
        _twos_exp += 1;
    }



    //
    // Apply G
    // Increments counts, and that's all.
    //
    void apply_G() {
        _twos_exp += 1;
    }



    //
    // Is Below
    // Compares the result of powers of 3 and 2, and returns true if 3^f < 2^k.  These have been precomputed and stored in an array
    // for us to lookup cheaply.
    //
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
