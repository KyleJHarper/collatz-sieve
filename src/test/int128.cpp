#include <cassert>
#include <cstdint>
#include <gmpxx.h>
#include <limits>
#include <stdexcept>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"
#include "../collatz/int128.hpp"




template<AnySupportedIntegral T>
void test_int128_parse_u128() {
    start_test(__func__);

    uint128_t x = Int128::parse_u128("1234567890");
    assert(x == uint128_t(1234567890));

    try {
        uint128_t y = Int128::parse_u128("123 nope 123");
        assert(false);
        assert(y == 0);
    } catch (std::invalid_argument& e) {
        assert(std::string(e.what()).find("Invalid digit in uint128 literal") != std::string::npos);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_int128_uint128_to_mpz() {
    start_test(__func__);

    // Return version
    uint128_t value = (uint128_t(1) << 64) + std::numeric_limits<uint64_t>::max();
    mpz_class value_mpz = (mpz_class(1) << 64) + std::numeric_limits<uint64_t>::max();
    assert(Int128::uint128_to_mpz(value) == value_mpz);

    // Out version.
    mpz_class out;
    Int128::uint128_to_mpz(value, out);
    assert(out == value_mpz);

    end_test();
}



template<AnySupportedIntegral T>
void test_int128_uint128_to_mpf() {
    start_test(__func__);

    // Return version (only)
    uint128_t value = (uint128_t(1) << 64) + std::numeric_limits<uint64_t>::max();
    mpf_class value_mpf = (mpf_class(1) << 64) + std::numeric_limits<uint64_t>::max();
    // Since it's derived from an integral, it avoids floating differences usually.
    // For safety, just assert neighboring integers.
    assert(Int128::uint128_to_mpf(value) -1 < value_mpf);
    assert(Int128::uint128_to_mpf(value) +1 > value_mpf);

    end_test();
}



template<AnySupportedIntegral T>
void test_int128_int128_to_string() {
    start_test(__func__);

    int128_t value = 0;
    for (int64_t i = -100000; i < 100000; i++) {
        value = i;
        assert(Int128::int128_to_string(value) == std::to_string(i));
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_int128_uint128_to_string() {
    start_test(__func__);

    uint128_t value = 0;
    for (uint64_t i = 0; i < 100000; i++) {
        value = i;
        assert(Int128::uint128_to_string(value) == std::to_string(i));
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_int128_str_to_uint128() {
    start_test(__func__);

    // Empty is an error.
    try {
        std::string s = "";
        uint128_t x = Int128::str_to_uint128(s);
        assert(false);
        assert(x == 42);
    } catch (std::invalid_argument& e) {
        assert(std::string(e.what()).find("Empty string for uint128_t") != std::string::npos);
    }

    // Invalid character is an error.
    try {
        std::string s = "123 nope 123";
        uint128_t x = Int128::str_to_uint128(s);
        assert(false);
        assert(x == 42);
    } catch (std::invalid_argument& e) {
        assert(std::string(e.what()).find("Invalid character in uint128_t") != std::string::npos);
    }

    // Loop through several tests.
    for (uint64_t i = 0; i < 100000; i++) {
        std::string s = std::to_string(i);
        uint128_t value = i;
        assert(Int128::str_to_uint128(s) == value);
    }

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_int128_parse_u128<T>();
    test_int128_uint128_to_mpz<T>();
    test_int128_uint128_to_mpf<T>();
    test_int128_int128_to_string<T>();
    test_int128_uint128_to_string<T>();
    test_int128_str_to_uint128<T>();
}



int main() {
    std::string name = "Int128";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
