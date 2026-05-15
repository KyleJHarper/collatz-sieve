#include <cassert>
#include <cstdint>
#include <gmp.h>
#include <stdexcept>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"
#include "../collatz/exponents.hpp"




template<AnySupportedIntegral T>
void test_exponents_max_pow2_under_pow_3() {
    start_test(__func__);

    assert(Exponents::MAX_POW2_UNDER_POW3_COUNT == 512);

    // Test all.  Zero is a special case
    size_t exp_2 = 0;
    size_t exp_3 = 0;
    mpz_class pow_2 = 0;
    mpz_class pow_3 = 0;
    const mpz_class mpz_two = 2;
    const mpz_class mpz_three = 3;
    while (exp_3 < Exponents::MAX_POW2_UNDER_POW3_COUNT) {
        mpz_pow_ui(pow_3.get_mpz_t(), mpz_three.get_mpz_t(), exp_3);
        while (pow_2 < pow_3) {
            exp_2++;
            mpz_pow_ui(pow_2.get_mpz_t(), mpz_two.get_mpz_t(), exp_2);
        }
        // pow_2 is now over pow_3 by one exponent, so drag it back.
        exp_2--;
        assert(exp_2 == Exponents::MAX_POW2_UNDER_POW3[exp_3]);
        exp_3++;
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_exponents_get_power_of_three() {
    start_test(__func__);

    mpz_class value = 0;
    const mpz_class mpz_three = 3;

    // 64-bit
    assert(Exponents::POW3_64BIT_ELEMENT_COUNT == 40);
    for (size_t exp = 0; exp < Exponents::POW3_64BIT_ELEMENT_COUNT; exp++) {
        mpz_pow_ui(value.get_mpz_t(), mpz_three.get_mpz_t(), exp);
        assert(value == Exponents::POW3_64BIT[exp]);
        assert(value == Exponents::get_power_of_three<uint64_t>(exp));
    }

    // 128-bit
    assert(Exponents::POW3_128BIT_ELEMENT_COUNT == 80);
    for (size_t exp = 0; exp < Exponents::POW3_128BIT_ELEMENT_COUNT; exp++) {
        mpz_pow_ui(value.get_mpz_t(), mpz_three.get_mpz_t(), exp);
        assert(value == Int128::uint128_to_mpz(Exponents::POW3_128BIT[exp]));
        assert(value == Int128::uint128_to_mpz(Exponents::get_power_of_three<uint128_t>(exp)));
    }

    // MPZ
    assert(Exponents::POW3_MPZ_ELEMENT_COUNT == 512);
    for (size_t exp = 0; exp < Exponents::POW3_MPZ_ELEMENT_COUNT; exp++) {
        mpz_pow_ui(value.get_mpz_t(), mpz_three.get_mpz_t(), exp);
        assert(value == Exponents::POW3_MPZ[exp]);
        assert(value == Exponents::get_power_of_three<mpz_class>(exp));
    }

    // Out of range checks.
    value = 42;
    try {
        assert(value == Exponents::get_power_of_three<uint64_t>(Exponents::POW3_64BIT_ELEMENT_COUNT + 1));
        assert(false);
    } catch (std::out_of_range& e) {
        assert(std::string(e.what()).find("Cannot get power of three for 64-bit integer with an exponent of") != std::string::npos);
    }
    try {
        assert(value == Int128::uint128_to_mpz(Exponents::get_power_of_three<uint128_t>(Exponents::POW3_128BIT_ELEMENT_COUNT + 1)));
        assert(false);
    } catch (std::out_of_range& e) {
        assert(std::string(e.what()).find("Cannot get power of three for 128-bit integer with an exponent of") != std::string::npos);
    }
    try {
        assert(value == Exponents::get_power_of_three<mpz_class>(Exponents::POW3_MPZ_ELEMENT_COUNT + 1));
        assert(false);
    } catch (std::out_of_range& e) {
        assert(std::string(e.what()).find("Cannot get power of three for MPZ integer with an exponent of") != std::string::npos);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_exponents_get_power_of_two() {
    start_test(__func__);

    mpz_class value = 0;
    const mpz_class mpz_two = 2;

    // 64-bit
    assert(Exponents::POW2_64BIT_ELEMENT_COUNT == 64);
    for (size_t exp = 0; exp < Exponents::POW2_64BIT_ELEMENT_COUNT; exp++) {
        mpz_pow_ui(value.get_mpz_t(), mpz_two.get_mpz_t(), exp);
        assert(value == Exponents::POW2_64BIT[exp]);
        assert(value == Exponents::get_power_of_two<uint64_t>(exp));
    }

    // 128-bit
    assert(Exponents::POW2_128BIT_ELEMENT_COUNT == 128);
    for (size_t exp = 0; exp < Exponents::POW2_128BIT_ELEMENT_COUNT; exp++) {
        mpz_pow_ui(value.get_mpz_t(), mpz_two.get_mpz_t(), exp);
        assert(value == Int128::uint128_to_mpz(Exponents::POW2_128BIT[exp]));
        assert(value == Int128::uint128_to_mpz(Exponents::get_power_of_two<uint128_t>(exp)));
    }

    // MPZ
    assert(Exponents::POW2_MPZ_ELEMENT_COUNT == 512);
    for (size_t exp = 0; exp < Exponents::POW2_MPZ_ELEMENT_COUNT; exp++) {
        mpz_pow_ui(value.get_mpz_t(), mpz_two.get_mpz_t(), exp);
        assert(value == Exponents::POW2_MPZ[exp]);
        assert(value == Exponents::get_power_of_two<mpz_class>(exp));
    }

    // Out of range checks.
    value = 42;
    try {
        assert(value == Exponents::get_power_of_two<uint64_t>(Exponents::POW2_64BIT_ELEMENT_COUNT + 1));
        assert(false);
    } catch (std::out_of_range& e) {
        assert(std::string(e.what()).find("Cannot get power of two for 64-bit integer with an exponent of") != std::string::npos);
    }
    try {
        assert(value == Int128::uint128_to_mpz(Exponents::get_power_of_two<uint128_t>(Exponents::POW2_128BIT_ELEMENT_COUNT + 1)));
        assert(false);
    } catch (std::out_of_range& e) {
        assert(std::string(e.what()).find("Cannot get power of two for 128-bit integer with an exponent of") != std::string::npos);
    }
    try {
        assert(value == Exponents::get_power_of_two<mpz_class>(Exponents::POW2_MPZ_ELEMENT_COUNT + 1));
        assert(false);
    } catch (std::out_of_range& e) {
        assert(std::string(e.what()).find("Cannot get power of two for MPZ integer with an exponent of") != std::string::npos);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_exponents_() {
    start_test(__func__);


    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_exponents_max_pow2_under_pow_3<T>();
    test_exponents_get_power_of_three<T>();
    test_exponents_get_power_of_two<T>();
}



int main() {
    std::string name = "Exponents";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
