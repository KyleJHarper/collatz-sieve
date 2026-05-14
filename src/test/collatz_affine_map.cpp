#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"
#include "../collatz/collatz_affine_map.hpp"
#include "../collatz/collatz.hpp"



template<AnySupportedIntegral T>
void test_collatz_affine_map_basics() {
    start_test(__func__);

    // All values should be zero to start.
    CollatzAffineMap<T> map;
    assert(map.get_twos_exp() == 0);
    assert(map.get_threes_exp() == 0);
    assert(map.get_constant_portion() == 0);

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_affine_map_reset() {
    start_test(__func__);

    // Apply some steps.
    CollatzAffineMap<T> map;
    map.apply_F();
    map.apply_G();
    map.apply_G();
    map.apply_F();
    map.apply_F();

    // Values should all be non-zero.
    assert(map.get_twos_exp() != 0);
    assert(map.get_threes_exp() != 0);
    assert(map.get_constant_portion() != 0);

    // Now reset.  Values should be zero again.
    map.reset();
    assert(map.get_twos_exp() == 0);
    assert(map.get_threes_exp() == 0);
    assert(map.get_constant_portion() == 0);

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_affine_map_apply_F() {
    start_test(__func__);

    // Single application.
    CollatzAffineMap<T> map;
    map.apply_F();
    assert(map.get_twos_exp() == 1);
    assert(map.get_threes_exp() == 1);
    assert(map.get_constant_portion() == 1);

    // Loop it a bunch.
    // Reminder: the constant portion is an integral, not fractional.  Shifting is leveraged after-the-fact to avoid floating ops.
    // Reminder: bit limits exist because this form of the affine map uses powers of 3, which are limited to 3^40 (64-bit).
    size_t max_shifts = Exponents::POW3_64BIT_ELEMENT_COUNT;
    if constexpr(Int128Integral<T>) {
        max_shifts = Exponents::POW3_128BIT_ELEMENT_COUNT;
    } else if constexpr(GMPIntegral<T>) {
        max_shifts = 255;
    }
    map.reset();
    T constant_portion = 0;
    for (size_t i = 1; i < max_shifts; i++) {
        constant_portion = (3 * constant_portion) + (T(1) << map.get_twos_exp());
        map.apply_F();
        assert(map.get_twos_exp() == i);
        assert(map.get_threes_exp() == i);
        assert(map.get_constant_portion() == constant_portion);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_affine_map_apply_G() {
    start_test(__func__);

    // Single application.
    CollatzAffineMap<T> map;
    map.apply_G();
    assert(map.get_twos_exp() == 1);
    assert(map.get_threes_exp() == 0);
    assert(map.get_constant_portion() == 0);

    // Loop it a bunch.
    // Reminder: the constant portion is an integral, not fractional.  Shifting is leveraged after-the-fact to avoid floating ops.
    // Reminder: bit limits exist because this form of the affine map uses powers of 3, which are limited to 3^40 (64-bit).
    size_t max_shifts = Exponents::POW2_64BIT_ELEMENT_COUNT;
    if constexpr(Int128Integral<T>) {
        max_shifts = Exponents::POW2_128BIT_ELEMENT_COUNT;
    } else if constexpr(GMPIntegral<T>) {
        max_shifts = 255;
    }
    map.reset();
    T constant_portion = 0;
    for (size_t i = 1; i < max_shifts; i++) {
        // Never changes... constant_portion = (3 * constant_portion) + (T(1) << map.get_twos_exp());
        map.apply_G();
        assert(map.get_twos_exp() == i);
        assert(map.get_threes_exp() == 0);
        assert(map.get_constant_portion() == constant_portion);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_affine_map_calculate() {
    start_test(__func__);

    // Known FG patterns and their associated initial values.
    CollatzAffineMap<T> map;
    T out;
    // 1
    map.reset();
    assert(map.calculate(1) == 1);
    map.calculate(1, out);
    assert(out == 1);
    // 2
    map.reset();
    map.apply_G();
    assert(map.calculate(2) == 1);
    map.calculate(2, out);
    assert(out == 1);
    // 3
    map.reset();
    map.apply_F();
    assert(map.calculate(3) == 5);
    map.calculate(3, out);
    assert(out == 5);
    // 4
    map.reset();
    map.apply_G();
    map.apply_G();
    assert(map.calculate(4) == 1);
    map.calculate(4, out);
    assert(out == 1);
    // 5
    map.reset();
    map.apply_F();
    map.apply_G();
    assert(map.calculate(5) == 4);
    map.calculate(5, out);
    assert(out == 4);
    // 6
    map.reset();
    map.apply_G();
    map.apply_F();
    assert(map.calculate(6) == 5);
    map.calculate(6, out);
    assert(out == 5);
    // 7
    map.reset();
    map.apply_F();
    map.apply_F();
    assert(map.calculate(7) == 17);
    map.calculate(7, out);
    assert(out == 17);

    // Test FG strings against larger values.
    // G ==> x / 2
    map.reset();
    map.apply_G();
    assert(map.calculate(2) == 1);
    assert(map.calculate(4) == 2);
    assert(map.calculate(6) == 3);
    assert(map.calculate(8) == 4);
    assert(map.calculate(10) == 5);
    assert(map.calculate(12) == 6);
    assert(map.calculate(14) == 7);
    assert(map.calculate(16) == 8);
    map.calculate(2, out);   assert(out == 1);
    map.calculate(4, out);   assert(out == 2);
    map.calculate(6, out);   assert(out == 3);
    map.calculate(8, out);   assert(out == 4);
    map.calculate(10, out);  assert(out == 5);
    map.calculate(12, out);  assert(out == 6);
    map.calculate(14, out);  assert(out == 7);
    map.calculate(16, out);  assert(out == 8);
    // F ==> (3x + 1) / 2
    map.reset();
    map.apply_F();
    assert(map.calculate(1) == 2);
    assert(map.calculate(3) == 5);
    assert(map.calculate(5) == 8);
    assert(map.calculate(7) == 11);
    assert(map.calculate(9) == 14);
    assert(map.calculate(11) == 17);
    assert(map.calculate(13) == 20);
    assert(map.calculate(15) == 23);
    map.calculate(1, out);   assert(out == 2);
    map.calculate(3, out);   assert(out == 5);
    map.calculate(5, out);   assert(out == 8);
    map.calculate(7, out);   assert(out == 11);
    map.calculate(9, out);   assert(out == 14);
    map.calculate(11, out);  assert(out == 17);
    map.calculate(13, out);  assert(out == 20);
    map.calculate(15, out);  assert(out == 23);
    // GG ==> (x / 2) / 2 ==> x / 4
    map.reset();
    map.apply_G();
    map.apply_G();
    assert(map.calculate(4) == 1);
    assert(map.calculate(8) == 2);
    assert(map.calculate(12) == 3);
    assert(map.calculate(16) == 4);
    assert(map.calculate(20) == 5);
    assert(map.calculate(24) == 6);
    assert(map.calculate(28) == 7);
    assert(map.calculate(32) == 8);
    map.calculate(4, out);   assert(out == 1);
    map.calculate(8, out);   assert(out == 2);
    map.calculate(12, out);  assert(out == 3);
    map.calculate(16, out);  assert(out == 4);
    map.calculate(20, out);  assert(out == 5);
    map.calculate(24, out);  assert(out == 6);
    map.calculate(28, out);  assert(out == 7);
    map.calculate(32, out);  assert(out == 8);
    // GF ==> ((x / 2) * 3 + 1) / 2
    map.reset();
    map.apply_G();
    map.apply_F();
    assert(map.calculate(6) == 5);
    assert(map.calculate(10) == 8);
    assert(map.calculate(14) == 11);
    assert(map.calculate(18) == 14);
    assert(map.calculate(22) == 17);
    assert(map.calculate(26) == 20);
    assert(map.calculate(30) == 23);
    map.calculate(6, out);   assert(out == 5);
    map.calculate(10, out);  assert(out == 8);
    map.calculate(14, out);  assert(out == 11);
    map.calculate(18, out);  assert(out == 14);
    map.calculate(22, out);  assert(out == 17);
    map.calculate(26, out);  assert(out == 20);
    map.calculate(30, out);  assert(out == 23);
    // FG ==> (((x * 3) + 1) / 2) / 2 ==> ((x * 3) + 1) / 4
    map.reset();
    map.apply_F();
    map.apply_G();
    assert(map.calculate(5) == 4);
    assert(map.calculate(9) == 7);
    assert(map.calculate(13) == 10);
    assert(map.calculate(17) == 13);
    assert(map.calculate(21) == 16);
    assert(map.calculate(25) == 19);
    assert(map.calculate(29) == 22);
    map.calculate(5, out);   assert(out == 4);
    map.calculate(9, out);   assert(out == 7);
    map.calculate(13, out);  assert(out == 10);
    map.calculate(17, out);  assert(out == 13);
    map.calculate(21, out);  assert(out == 16);
    map.calculate(25, out);  assert(out == 19);
    map.calculate(29, out);  assert(out == 22);
    // FF ==> (((((x * 3) + 1) / 2) * 3) + 1) / 2
    map.reset();
    map.apply_F();
    map.apply_F();
    assert(map.calculate(7) == 17);
    assert(map.calculate(11) == 26);
    assert(map.calculate(15) == 35);
    assert(map.calculate(19) == 44);
    assert(map.calculate(23) == 53);
    assert(map.calculate(27) == 62);
    assert(map.calculate(31) == 71);
    map.calculate(7, out);  assert(out == 17);
    map.calculate(11, out);  assert(out == 26);
    map.calculate(15, out);  assert(out == 35);
    map.calculate(19, out);  assert(out == 44);
    map.calculate(23, out);  assert(out == 53);
    map.calculate(27, out);  assert(out == 62);
    map.calculate(31, out);  assert(out == 71);

    // Overflows should be caught for fixed-width types.
    if constexpr (FixedWidthIntegral<T>) {
        size_t max_shifts = Exponents::POW2_64BIT_ELEMENT_COUNT;
        if constexpr(Int128Integral<T>) {
            max_shifts = Exponents::POW2_128BIT_ELEMENT_COUNT;
        }

        // Create an out-of-range exception for powers of three.
        map.reset();
        for (size_t i = 1; i < max_shifts + 1; i++) {
            map.apply_F();
        }
        try {
            map.calculate(1);
            assert(false);
        } catch (std::out_of_range& e) {
            assert(std::string(e.what()).find("Cannot get power of three") != std::string::npos);
        }

        // Create an overflow by any map (including a valid one) operating on a large integer.
        map.reset();
        map.apply_F();
        try {
            T junk = map.calculate(std::numeric_limits<T>::max());
            assert(false);
            junk++;
        } catch (std::overflow_error& e) {
            assert(std::string(e.what()).find("Overflow in CollatzAffineMap calculate() method") != std::string::npos);
        }
    }

    // Test with a known FG sequence.
    Collatz<T> c(17);
    assert(c.get_fg_chain_string() == "FGFGGFGGGF");
    map.reset();
    map.apply_F();
    assert(map.calculate(17) == 26);
    map.apply_G();
    assert(map.calculate(17) == 13);
    map.apply_F();
    assert(map.calculate(17) == 20);
    map.apply_G();
    assert(map.calculate(17) == 10);
    map.apply_G();
    assert(map.calculate(17) == 5);
    map.apply_F();
    assert(map.calculate(17) == 8);
    map.apply_G();
    assert(map.calculate(17) == 4);
    map.apply_G();
    assert(map.calculate(17) == 2);
    map.apply_G();
    assert(map.calculate(17) == 1);

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_affine_map_is_below() {
    start_test(__func__);

    CollatzAffineMap<T> map;
    // G == contractive
    map.reset();
    map.apply_G();
    assert(map.is_below(16) == true);
    // F == expansive
    map.reset();
    map.apply_F();
    assert(map.is_below(15) == false);
    // GG == contractive
    map.reset();
    map.apply_G();
    map.apply_G();
    assert(map.is_below(16) == true);
    // GF == contractive
    map.reset();
    map.apply_G();
    map.apply_G();
    assert(map.is_below(10) == true);
    // FG  == contractive
    map.reset();
    map.apply_F();
    map.apply_G();
    assert(map.is_below(9) == true);
    // FF == expansive
    map.reset();
    map.apply_F();
    map.apply_F();
    assert(map.is_below(15) == false);

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_collatz_affine_map_basics<T>();
    test_collatz_affine_map_reset<T>();
    test_collatz_affine_map_apply_F<T>();
    test_collatz_affine_map_apply_G<T>();
    test_collatz_affine_map_calculate<T>();
    test_collatz_affine_map_is_below<T>();
}






int main() {
    std::string name = "CollatzAffineMap";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
