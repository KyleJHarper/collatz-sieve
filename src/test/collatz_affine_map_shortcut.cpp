#include <cassert>
#include <cstdint>
#include <stdexcept>
#include "helpers.hpp"
#include "../collatz/collatz_affine_map.hpp"




void test_collatz_affine_map_shortcut_basics() {
    start_test(__func__);

    // All values should be zero to start.
    CollatzAffineMapShortcut map;
    assert(map.get_twos_exp() == 0);
    assert(map.get_threes_exp() == 0);

    // Statics
    assert(map.MAX_SERIAL_F_TESTED == 10000);

    end_test();
}



void test_collatz_affine_map_shortcut_reset() {
    start_test(__func__);

    // Apply some steps.
    CollatzAffineMapShortcut map;
    map.apply_F();
    map.apply_G();
    map.apply_G();
    map.apply_F();
    map.apply_F();

    // Values should all be non-zero.
    assert(map.get_twos_exp() != 0);
    assert(map.get_threes_exp() != 0);

    // Now reset.  Values should be zero again.
    map.reset();
    assert(map.get_twos_exp() == 0);
    assert(map.get_threes_exp() == 0);

    end_test();
}



void test_collatz_affine_map_shortcut_apply_F() {
    start_test(__func__);

    // Single application.
    CollatzAffineMapShortcut map;
    map.apply_F();
    assert(map.get_twos_exp() == 1);
    assert(map.get_threes_exp() == 1);

    // Loop it a bunch.
    // Reminder: the constant portion is an integral, not fractional.  Shifting is leveraged after-the-fact to avoid floating ops.
    // Reminder: bit limits exist because this form of the affine map uses powers of 3, which are limited to 3^40 (64-bit).
    size_t max_shifts = CollatzAffineMapShortcut::MAX_SERIAL_F_TESTED;
    map.reset();
    for (size_t i = 1; i <= max_shifts; i++) {
        map.apply_F();
        assert(map.get_twos_exp() == i);
        assert(map.get_threes_exp() == i);
    }

    // The next F step should throw an out of range error.
    try {
        map.apply_F();
        assert(false);
    } catch (std::out_of_range& e) {
        assert(std::string(e.what()).find("CollatzAffineMapShortcut cannot apply another F step because the maximum") != std::string::npos);
    }

    end_test();
}



void test_collatz_affine_map_shortcut_apply_G() {
    start_test(__func__);

    // Single application.
    CollatzAffineMapShortcut map;
    map.apply_G();
    assert(map.get_twos_exp() == 1);
    assert(map.get_threes_exp() == 0);

    // Loop it a bunch.
    // Reminder: the constant portion is an integral, not fractional.  Shifting is leveraged after-the-fact to avoid floating ops.
    // Reminder: bit limits exist because this form of the affine map uses powers of 3, which are limited to 3^40 (64-bit).
    size_t max_shifts = CollatzAffineMapShortcut::MAX_SERIAL_F_TESTED;
    map.reset();
    for (size_t i = 1; i < max_shifts; i++) {
        map.apply_G();
        assert(map.get_twos_exp() == i);
        assert(map.get_threes_exp() == 0);
    }

    end_test();
}



void test_collatz_affine_map_shortcut_is_below() {
    start_test(__func__);

    CollatzAffineMapShortcut map;
    // G == contractive
    map.reset();
    map.apply_G();
    assert(map.is_below() == true);
    // F == expansive
    map.reset();
    map.apply_F();
    assert(map.is_below() == false);
    // GG == contractive
    map.reset();
    map.apply_G();
    map.apply_G();
    assert(map.is_below() == true);
    // GF == contractive
    map.reset();
    map.apply_G();
    map.apply_G();
    assert(map.is_below() == true);
    // FG  == contractive
    map.reset();
    map.apply_F();
    map.apply_G();
    assert(map.is_below() == true);
    // FF == expansive
    map.reset();
    map.apply_F();
    map.apply_F();
    assert(map.is_below() == false);

    // Out of range check.
    map.reset();
    for (size_t i = 1; i <= Exponents::MAX_POW2_UNDER_POW3_COUNT; i++) {
        map.apply_F();
    }
    // Works still.
    assert(map.is_below() == false);
    // Go one more step.
    map.apply_F();
    // Now is_below throws.
    try {
        assert(map.is_below() == false);
        assert(false);
    } catch (std::out_of_range& e) {
        assert(std::string(e.what()).find("Cannot lookup MAX_POW2_UNDER_POW3 with _threes_exp of") != std::string::npos);
    }

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_collatz_affine_map_shortcut_basics();
    test_collatz_affine_map_shortcut_reset();
    test_collatz_affine_map_shortcut_apply_F();
    test_collatz_affine_map_shortcut_apply_G();
    test_collatz_affine_map_shortcut_is_below();
}



int main() {
    std::string name = "CollatzAffineMapShortcut";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
