#pragma once
#include "gmpxx.h"
#include <array>
#include "concepts.hpp"
#include "udl.hpp"




/**
* @namespace CollatzConstants
* @brief Constants and helpers for Collatz sequence processing.
*/
namespace CollatzConstants {
    /// @brief Highest known empircally tested level.  See [Barina](https://link.springer.com/article/10.1007/s11227-025-07337-0).
    static const level_t LARGEST_EMPIRICALLY_TESTED_LEVEL = 72;

    // GMP will sometimes alloc() if you operate on a non-GMP (e.g.: ui) value.
    static const mpz_class MPZ_ONE = 1;    ///< Constant `mpz_class(1)` to prevent allocs in some cases.
    static const mpz_class MPZ_TWO = 2;    ///< Constant `mpz_class(2)` to prevent allocs in some cases.
    static const mpz_class MPZ_THREE = 3;  ///< Constant `mpz_class(3)` to prevent allocs in some cases.

    // GMP float-style values used in a lot of calculations.
    static const mpf_class MPF_HALF = 0.5;  ///< Constant `mpf_class(0.5)` to prevent allocs in some cases.
    static const mpf_class MPF_ONE = 1;     ///< Constant `mpf_class(1)` to prevent allocs in some cases.
    static const mpf_class MPF_TWO = 2;     ///< Constant `mpf_class(2)` to prevent allocs in some cases.
    static const mpf_class MPF_THREE = 3;   ///< Constant `mpf_class(3)` to prevent allocs in some cases.



    /**
    * @brief Stores the highest integer which can take 3x + 1 without overflowing a given bit width.
    * @note This only supports 8, 16, 32, 64, and 128 bit widths.
    * @note This only supports unsigned types.
    */
    constexpr std::array<uint128_t, 5> MAX_3XP1 = {
        ((uint128_t(1) <<  8) - 1 - 1) / 3,   // 8 bit
        ((uint128_t(1) << 16) - 1 - 1) / 3,   // 16 bit
        ((uint128_t(1) << 32) - 1 - 1) / 3,   // 32 bit
        ((uint128_t(1) << 64) - 1 - 1) / 3,   // 64 bit
        ((uint128_t(1) << 127) + ((uint128_t(1) << 127) - 1) - 1) / 3  // 128 bit
    };



    /**
    * @brief Helper to return the correct `MAX_3XP1` for the type `T`.
    * @tparam T Any supported integral (see concepts.hpp).
    */
    template<FixedWidthIntegral T>
    inline constexpr uint128_t get_max_3xp1() {
        switch (sizeof(T) * 8) {
            case   8: return MAX_3XP1[0];
            case  16: return MAX_3XP1[1];
            case  32: return MAX_3XP1[2];
            case  64: return MAX_3XP1[3];
            case 128: return MAX_3XP1[4];
            default: throw std::logic_error("Bit size not supported: " + std::to_string(sizeof(T) * 8));
        }
    }



    /**
    * @brief Precomputed maximum initial values for `2^bit`.  Values over this can overflow during their sequence.
    *
    * Each array index represents the bit size of a given data type.  For example, uint32_t == 32 bits, so the maximum initial
    * value that can be used without overflowing during a sequence would be `MAX_INITIAL_VALUE_BY_BIT[32]`.
    *
    * Consequently, each of these values can be increased by 1 to find the first overflow point, fyi.
    *
    * \par Example
    * Given `uint8_t` is 8 bits, the largest value it can hold is 255 (2^8 - 1).  The max initial value shown in the table is 26.
    * No value (step) created by an initial value at or below this point (1-26) will cause an overflow when its Collatz sequence is
    * processed.  The next value (27) will:
    *   * 27, 82, 41, 124, 62, 31, 94, 47, 142, 71, 214, 107, 322 (overflow)
    *
    * @warning These values have been tested up to 2^109.  That point is used for 2^110 - 2^128 as a safety precaution until the
    * remaining values can be found.
    *
    * \showinitializer
    */
    constexpr std::array<uint128_t, 129> MAX_INITIAL_VALUE_BY_BIT = {
        0,  // 0
        1,  // 1
        2,  // 2
        2,  // 3
        2,  // 4
        6,  // 5
        14,  // 6
        14,  // 7
        26,  // 8
        26,  // 9
        26,  // 10
        26,  // 11
        26,  // 12
        26,  // 13
        446,  // 14
        446,  // 15
        702,  // 16
        702,  // 17
        1818,  // 18
        1818,  // 19
        1818,  // 20
        4254,  // 21
        4254,  // 22
        9662,  // 23
        9662,  // 24
        20894,  // 25
        26622,  // 26
        60974,  // 27
        60974,  // 28
        60974,  // 29
        77670,  // 30
        113382,  // 31
        159486,  // 32
        159486,  // 33
        159486,  // 34
        665214,  // 35
        1042430,  // 36
        1212414,  // 37
        2684646,  // 38
        3041126,  // 39
        4637978,  // 40
        5656190,  // 41
        6416622,  // 42
        6631674,  // 43
        6631674,  // 44
        6631674,  // 45
        19638398,  // 46
        19638398,  // 47
        19638398,  // 48
        80049390,  // 49
        80049390,  // 50
        120080894,  // 51
        210964382,  // 52
        319804830,  // 53
        319804830,  // 54
        319804830,  // 55
        319804830,  // 56
        319804830,  // 57
        319804830,  // 58
        319804830,  // 59
        319804830,  // 60
        1410123942,  // 61
        1410123942,  // 62
        8528817510,  // 63
        12327829502,  // 64
        "23035537406"_u128,  // 65
        "45871962270"_u128,  // 66
        "59152641054"_u128,  // 67
        "70141259774"_u128,  // 68
        "77566362558"_u128,  // 69
        "110243094270"_u128,  // 70
        "272025660542"_u128,  // 71
        "272025660542"_u128,  // 72
        "272025660542"_u128,  // 73
        "272025660542"_u128,  // 74
        "446559217278"_u128,  // 75
        "567839862630"_u128,  // 76
        "871673828442"_u128,  // 77
        "871673828442"_u128,  // 78
        "2674309547646"_u128,  // 79
        "3716509988198"_u128,  // 80
        "3716509988198"_u128,  // 81
        "3716509988198"_u128,  // 82
        "3716509988198"_u128,  // 83
        "3716509988198"_u128,  // 84
        "3716509988198"_u128,  // 85
        "3716509988198"_u128,  // 86
        "3716509988198"_u128,  // 87
        "64848224337146"_u128,  // 88
        "64848224337146"_u128,  // 89
        "64848224337146"_u128,  // 90
        "116050121715710"_u128,  // 91
        "201321227677934"_u128,  // 92
        "394491988532894"_u128,  // 93
        "406738920960666"_u128,  // 94
        "613450176662510"_u128,  // 95
        "1254251874774374"_u128,  // 96
        "1254251874774374"_u128,  // 97
        "1254251874774374"_u128,  // 98
        "1254251874774374"_u128,  // 99
        "1254251874774374"_u128,  // 100
        "1254251874774374"_u128,  // 101
        "8562235014026654"_u128,  // 102
        "8562235014026654"_u128,  // 103
        "8562235014026654"_u128,  // 104
        "10709980568908646"_u128,  // 105
        "10709980568908646"_u128,  // 106
        "10709980568908646"_u128,  // 107
        "10709980568908646"_u128,  // 108

        // The following are placeholders until they can be computed.
        // This is the highest value computed without hitting the next 2^k max IV.

        // "78000079941432167"_u128,  // 109   Latest value from RTX5060 work.
        "78000079941432167"_u128,  // 109
        "78000079941432167"_u128,  // 110
        "78000079941432167"_u128,  // 111
        "78000079941432167"_u128,  // 112
        "78000079941432167"_u128,  // 113
        "78000079941432167"_u128,  // 114
        "78000079941432167"_u128,  // 115
        "78000079941432167"_u128,  // 116
        "78000079941432167"_u128,  // 117
        "78000079941432167"_u128,  // 118
        "78000079941432167"_u128,  // 119
        "78000079941432167"_u128,  // 120
        "78000079941432167"_u128,  // 121
        "78000079941432167"_u128,  // 122
        "78000079941432167"_u128,  // 123
        "78000079941432167"_u128,  // 124
        "78000079941432167"_u128,  // 125
        "78000079941432167"_u128,  // 126
        "78000079941432167"_u128,  // 127
        "78000079941432167"_u128,  // 128
    };



    /**
    * @brief Get the maximum number of bits that type `T` can look up in the `MAX_INITIAL_VALUE_BY_BIT`.
    *
    * Normally, a simple `sizeof(T) * 8` works, but there's a check for `mpz_class`.  Wrapping it all in a constexpr helper lets
    * callers avoid the headache of juggling it directly, and frees this namespace to change implementation details later.
    *
    * @tparam T Any supported integral (see concepts.hpp).
    */
    template<AnySupportedIntegral T>
    inline constexpr size_t get_max_bits_for_max_initial_value_by_type() {
        if constexpr(FixedWidthIntegral<T>) {
            if constexpr(sizeof(T) <= 16) {
                return sizeof(T) * 8;
            }
        } else if constexpr(GMPIntegral<T>) {
            return MAX_INITIAL_VALUE_BY_BIT.size() - 1;
        }
    }



    /**
    * @brief Return the max initial value from the precomputed table for a given bit size.
    * @tparam T Any supported integral (see concepts.hpp).
    * @param bit_size The number of bits in question to look up in the `MAX_INITIAL_VALUE_BY_BIT` table.
    */
    template<AnySupportedIntegral T>
    inline constexpr T get_max_initial_value_by_bit(size_t bit_size) {
        // Safety Check
        constexpr size_t max_bits = get_max_bits_for_max_initial_value_by_type<T>();
        if (bit_size > max_bits) {
            throw std::out_of_range("Max initial value for bit size " + std::to_string(bit_size) + " not found because it exceeds max bits: " + std::to_string(max_bits) + ".");
        }

        if constexpr(FixedWidthIntegral<T>) {
            return static_cast<T>(MAX_INITIAL_VALUE_BY_BIT[bit_size]);
        } else if constexpr(GMPIntegral<T>) {
            return Int128::uint128_to_mpz(MAX_INITIAL_VALUE_BY_BIT[bit_size]);
        }
    }



    /**
    * @brief Return the max initial value from the precomputed table for the type `T` given.
    * @tparam T Any supported integral (see concepts.hpp).
    */
    template<AnySupportedIntegral T>
    inline constexpr T get_max_initial_value_by_type() {
        constexpr size_t max_bits = GMPIntegral<T> ? 128 : sizeof(T) * 8;
        return get_max_initial_value_by_bit<T>(max_bits);
    }
}
