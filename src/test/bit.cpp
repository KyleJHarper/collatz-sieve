#include <cassert>
#include <cstdint>
#include <limits>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"
#include "../collatz/bit.hpp"



void test_bit_bitreverse8() {
    start_test(__func__);

    for (uint8_t value = 0; value < std::numeric_limits<uint8_t>::max(); value++) {
        assert(Bit::bitreverse8(Bit::bitreverse8(value)) == value);
    }

    end_test();
}



void test_bit_bitreverse16() {
    start_test(__func__);

    for (uint16_t value = 0; value < std::numeric_limits<uint16_t>::max(); value++) {
        assert(Bit::bitreverse16(Bit::bitreverse16(value)) == value);
    }

    end_test();
}



void test_bit_bitreverse32() {
    start_test(__func__);

    for (uint32_t value = 0; value < 1000000; value++) {
        assert(Bit::bitreverse32(Bit::bitreverse32(value)) == value);
    }

    end_test();
}



void test_bit_bitreverse64() {
    start_test(__func__);

    for (uint64_t value = 0; value < 1000000; value++) {
        assert(Bit::bitreverse64(Bit::bitreverse64(value)) == value);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_bit_st_bit_reverse_full() {
    start_test(__func__);

    if constexpr(FixedWidthIntegral<T>) {
        // Since the values are width-dependent, we'll just double-reverse values to ensure they come back correctly.
        for (T value = 0; value <= 1000000; value++) {
            assert(Bit::st_bit_reverse_full(Bit::st_bit_reverse_full(value)) == value);
        }
    } else if constexpr (GMPIntegral<T>) {
        // There's no full bit reversal for GMP.
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_bit_st_reverse_low_bits() {
    start_test(__func__);

    // Return version.
    T value = 421;  // Binary: 110100101
    assert(Bit::st_reverse_low_bits(value, 1) == 1);   //         1
    assert(Bit::st_reverse_low_bits(value, 2) == 2);   //        10
    assert(Bit::st_reverse_low_bits(value, 3) == 5);   //       101
    assert(Bit::st_reverse_low_bits(value, 4) == 10);  //      1010
    assert(Bit::st_reverse_low_bits(value, 5) == 20);  //     10100
    assert(Bit::st_reverse_low_bits(value, 6) == 41);  //    101001
    assert(Bit::st_reverse_low_bits(value, 7) == 82);  //   1010010
    assert(Bit::st_reverse_low_bits(value, 8) == 165); //  10100101
    assert(Bit::st_reverse_low_bits(value, 9) == 331); // 101001011

    // Out version.
    T out = 0;
    Bit::st_reverse_low_bits(value, 1, out);  assert(out == 1);   //         1
    Bit::st_reverse_low_bits(value, 2, out);  assert(out == 2);   //        10
    Bit::st_reverse_low_bits(value, 3, out);  assert(out == 5);   //       101
    Bit::st_reverse_low_bits(value, 4, out);  assert(out == 10);  //      1010
    Bit::st_reverse_low_bits(value, 5, out);  assert(out == 20);  //     10100
    Bit::st_reverse_low_bits(value, 6, out);  assert(out == 41);  //    101001
    Bit::st_reverse_low_bits(value, 7, out);  assert(out == 82);  //   1010010
    Bit::st_reverse_low_bits(value, 8, out);  assert(out == 165); //  10100101
    Bit::st_reverse_low_bits(value, 9, out);  assert(out == 331); // 101001011

    end_test();
}



template<AnySupportedIntegral T>
void test_bit_count_trailing_zeros() {
    start_test(__func__);

    // Use 'int' because ctz() does, and Bit respects that.
    int max_shifts = (sizeof(T) * 8) - 1;
    if constexpr(GMPIntegral<T>) {
        max_shifts = 255;
    }
    for(int shifts = 0; shifts <= max_shifts; shifts++) {
        T value = T(1) << shifts;
        assert(Bit::count_trailing_zeros(value) == shifts);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_bit_count_trailing_ones() {
    start_test(__func__);

    // Use 'int' because ctz() does (which we use with inverse: ~n), and Bit respects that.
    int max_shifts = (sizeof(T) * 8) - 1;
    if constexpr(GMPIntegral<T>) {
        max_shifts = 255;
    }
    for(int shifts = 0; shifts <= max_shifts; shifts++) {
        T value = T(1) << shifts;
        T inverse_value = ~value;
        assert(Bit::count_trailing_ones(inverse_value) == shifts);
    }

    // Zero is a special case, which is undefind/UB.  We assert it should be 0.
    T value = 0;
    assert(Bit::count_trailing_zeros(value) == 0);

    end_test();
}



template<AnySupportedIntegral T>
void test_bit_byteswap() {
    start_test(__func__);

    // Single bytes don't swap.
    uint8_t value_8 = 123;
    assert(Bit::byteswap(value_8) == value_8);

    // 16-bit.
    uint16_t value_16 = 0x1234;
    assert(Bit::byteswap(value_16) == static_cast<uint16_t>(0x3412));

    uint32_t value_32 = 0x12345678;
    assert(Bit::byteswap(value_32) == static_cast<uint32_t>(0x78563412));

    uint64_t value_64 = 0x0123456789ABCDEFULL;
    assert(Bit::byteswap(value_64) == static_cast<uint64_t>(0xEFCDAB8967452301ULL));

    uint128_t value = (uint128_t(0x0011223344556677ULL) << 64) | uint128_t(0x8899AABBCCDDEEFFULL);
    uint128_t swapped = Bit::byteswap(value);
    uint64_t high = static_cast<uint64_t>(swapped >> 64);
    uint64_t low  = static_cast<uint64_t>(swapped);
    assert(high == 0xFFEEDDCCBBAA9988ULL);
    assert(low  == 0x7766554433221100ULL);

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_bit_bitreverse8();
    test_bit_bitreverse16();
    test_bit_bitreverse32();
    test_bit_bitreverse64();
    test_bit_st_bit_reverse_full<T>();
    test_bit_st_reverse_low_bits<T>();
    test_bit_count_trailing_zeros<T>();
    test_bit_count_trailing_ones<T>();
    test_bit_byteswap<T>();
}



int main() {
    std::string name = "Bit";
    preamble(name);
    std::cout << "Need to do count_leading_zeros and the nonzero variants\n";
    assert(false);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
