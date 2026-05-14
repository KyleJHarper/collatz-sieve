#include <cassert>
#include <cstdint>
#include <stdexcept>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"
#include "../collatz/collatz_constants.hpp"



void test_collatz_constants_basics() {
    start_test(__func__);

    assert(CollatzConstants::LARGEST_EMPIRICALLY_TESTED_LEVEL == 72);
    assert(CollatzConstants::MPZ_ONE == 1);
    assert(CollatzConstants::MPZ_TWO == 2);
    assert(CollatzConstants::MPZ_THREE == 3);
    // MPF types never seem to line up perfectly due to their floating nature.  Ignoring them.

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_constants_max_3xp1() {
    start_test(__func__);

    assert(CollatzConstants::MAX_3XP1.size() == 5);
    for (size_t i = 0; i < 5; i++) {
        size_t bits = 8 * (1ULL << i);
        mpz_class value = ((mpz_class(1) << bits) - 1 - 1) / 3;
        assert(Int128::uint128_to_mpz(CollatzConstants::MAX_3XP1[i]) == value);
    }

    // Get correct bits by type T.
    assert(CollatzConstants::get_max_3xp1<uint8_t>() == CollatzConstants::MAX_3XP1[0]);
    assert(CollatzConstants::get_max_3xp1<uint16_t>() == CollatzConstants::MAX_3XP1[1]);
    assert(CollatzConstants::get_max_3xp1<uint32_t>() == CollatzConstants::MAX_3XP1[2]);
    assert(CollatzConstants::get_max_3xp1<uint64_t>() == CollatzConstants::MAX_3XP1[3]);
    assert(CollatzConstants::get_max_3xp1<uint128_t>() == CollatzConstants::MAX_3XP1[4]);
    if constexpr(FixedWidthIntegral<T>) {
        uint128_t expected_value;
        switch(sizeof(T) * 8) {
            case  8:
                expected_value = CollatzConstants::MAX_3XP1[0];
                break;
            case  16:
                expected_value = CollatzConstants::MAX_3XP1[1];
                break;
            case  32:
                expected_value = CollatzConstants::MAX_3XP1[2];
                break;
            case  64:
                expected_value = CollatzConstants::MAX_3XP1[3];
                break;
            case  128:
                expected_value = CollatzConstants::MAX_3XP1[4];
                break;
            default:
                assert(false);
        }
        assert(CollatzConstants::get_max_3xp1<T>() == expected_value);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_constants_get_max_bits_for_max_initial_value_by_type() {
    start_test(__func__);

    assert(CollatzConstants::MAX_INITIAL_VALUE_BY_BIT.size() == 129);
    assert(CollatzConstants::get_max_bits_for_max_initial_value_by_type<uint8_t>() == 8);
    assert(CollatzConstants::get_max_bits_for_max_initial_value_by_type<uint16_t>() == 16);
    assert(CollatzConstants::get_max_bits_for_max_initial_value_by_type<uint32_t>() == 32);
    assert(CollatzConstants::get_max_bits_for_max_initial_value_by_type<uint64_t>() == 64);
    assert(CollatzConstants::get_max_bits_for_max_initial_value_by_type<uint128_t>() == 128);
    if constexpr(FixedWidthIntegral<T>) {
        assert(CollatzConstants::get_max_bits_for_max_initial_value_by_type<T>() == sizeof(T) * 8);
    } else if constexpr(GMPIntegral<T>) {
        assert(CollatzConstants::get_max_bits_for_max_initial_value_by_type<T>() == CollatzConstants::MAX_INITIAL_VALUE_BY_BIT.size() - 1);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_constants_get_max_initial_value_by_bit() {
    start_test(__func__);

    // 8 bit
    assert(CollatzConstants::get_max_initial_value_by_bit<uint8_t>(8) == CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[8]);
    try {
        assert(CollatzConstants::get_max_initial_value_by_bit<uint8_t>(9) == CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[9]);
        assert(false);
    } catch (std::out_of_range& e) {
        assert(std::string(e.what()).find("Max initial value for bit size") != std::string::npos);
    }
    // 16 bit
    assert(CollatzConstants::get_max_initial_value_by_bit<uint16_t>(16) == CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[16]);
    try {
        assert(CollatzConstants::get_max_initial_value_by_bit<uint16_t>(17) == CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[17]);
        assert(false);
    } catch (std::out_of_range& e) {
        assert(std::string(e.what()).find("Max initial value for bit size") != std::string::npos);
    }
    // 32 bit
    assert(CollatzConstants::get_max_initial_value_by_bit<uint32_t>(32) == CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[32]);
    try {
        assert(CollatzConstants::get_max_initial_value_by_bit<uint32_t>(33) == CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[33]);
        assert(false);
    } catch (std::out_of_range& e) {
        assert(std::string(e.what()).find("Max initial value for bit size") != std::string::npos);
    }
    // 64 bit
    assert(CollatzConstants::get_max_initial_value_by_bit<uint64_t>(64) == CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[64]);
    try {
        assert(CollatzConstants::get_max_initial_value_by_bit<uint64_t>(65) == CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[65]);
        assert(false);
    } catch (std::out_of_range& e) {
        assert(std::string(e.what()).find("Max initial value for bit size") != std::string::npos);
    }
    // 128 bit
    assert(CollatzConstants::get_max_initial_value_by_bit<uint128_t>(128) == CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[128]);
    try {
        assert(CollatzConstants::get_max_initial_value_by_bit<uint128_t>(129) == CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[129]);
        assert(false);
    } catch (std::out_of_range& e) {
        assert(std::string(e.what()).find("Max initial value for bit size") != std::string::npos);
    }
    // GMP
    assert(CollatzConstants::get_max_initial_value_by_bit<mpz_class>(8) == Int128::uint128_to_mpz(CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[8]));
    assert(CollatzConstants::get_max_initial_value_by_bit<mpz_class>(16) == Int128::uint128_to_mpz(CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[16]));
    assert(CollatzConstants::get_max_initial_value_by_bit<mpz_class>(32) == Int128::uint128_to_mpz(CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[32]));
    assert(CollatzConstants::get_max_initial_value_by_bit<mpz_class>(64) == Int128::uint128_to_mpz(CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[64]));
    assert(CollatzConstants::get_max_initial_value_by_bit<mpz_class>(128) == Int128::uint128_to_mpz(CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[128]));
    try {
        assert(CollatzConstants::get_max_initial_value_by_bit<mpz_class>(129) == Int128::uint128_to_mpz(CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[129]));
        assert(false);
    } catch (std::out_of_range& e) {
        assert(std::string(e.what()).find("Max initial value for bit size") != std::string::npos);
    }
    // T
    if constexpr(FixedWidthIntegral<T>) {
        size_t max_bits = sizeof(T) * 8;
        assert(CollatzConstants::get_max_initial_value_by_bit<T>(max_bits) == CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[max_bits]);
        try {
            assert(CollatzConstants::get_max_initial_value_by_bit<T>(max_bits + 1) == CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[max_bits + 1]);
            assert(false);
        } catch (std::out_of_range& e) {
            assert(std::string(e.what()).find("Max initial value for bit size") != std::string::npos);
        }
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_constants_get_max_initial_value_by_type() {
    start_test(__func__);

    assert(CollatzConstants::get_max_initial_value_by_type<uint8_t>() == CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[8]);
    assert(CollatzConstants::get_max_initial_value_by_type<uint16_t>() == CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[16]);
    assert(CollatzConstants::get_max_initial_value_by_type<uint32_t>() == CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[32]);
    assert(CollatzConstants::get_max_initial_value_by_type<uint64_t>() == CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[64]);
    assert(CollatzConstants::get_max_initial_value_by_type<uint128_t>() == CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[128]);
    if constexpr(FixedWidthIntegral<T>) {
        size_t bits = sizeof(T) * 8;
        assert(CollatzConstants::get_max_initial_value_by_type<T>() == CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[bits]);
    } else if constexpr (GMPIntegral<T>) {
        assert(CollatzConstants::get_max_initial_value_by_type<T>() == Int128::uint128_to_mpz(CollatzConstants::MAX_INITIAL_VALUE_BY_BIT[128]));
    }

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_collatz_constants_basics();
    test_collatz_constants_max_3xp1<T>();
    test_collatz_constants_get_max_bits_for_max_initial_value_by_type<T>();
    test_collatz_constants_get_max_initial_value_by_bit<T>();
    test_collatz_constants_get_max_initial_value_by_type<T>();
}



int main() {
    std::string name = "CollatzConstants";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
