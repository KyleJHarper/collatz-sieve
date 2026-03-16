#pragma once
#include "gmpxx.h"
#include <array>
#include "concepts.hpp"




//
// Constants and helpers for our sequences.
//
namespace CollatzConstants {
    // Highest known empircally tested level.  Barina: https://link.springer.com/article/10.1007/s11227-025-07337-0
    static const uint64_t LARGEST_EPIRICALLY_TESTED_LEVEL = 72;  // 2^71


    // GMP will sometimes alloc() if you operate on a non-GMP (e.g.: ui) value.
    static const mpz_class MPZ_ONE = 1;
    static const mpz_class MPZ_TWO = 2;
    static const mpz_class MPZ_THREE = 3;

    // GMP float-style values used in a lot of calculations.
    static const mpf_class MPF_ONE = 1;
    static const mpf_class MPF_TWO = 2;
    static const mpf_class MPF_THREE = 3;
    static const mpf_class MPF_HALF = 0.5;

    // Let's lock in what "odd" and "even" mean.
    constexpr bool ODD = true;
    constexpr bool EVEN = false;

    // Trying to perform 3X+1 on any value higher than this would overflow a 64-bit unsigned integer.
    constexpr std::array<uint128_t, 5> MAX_3XP1 = {
        ((uint128_t(1) <<  8) - 1 - 1) / 3,   // 8 bit
        ((uint128_t(1) << 16) - 1 - 1) / 3,   // 16 bit
        ((uint128_t(1) << 32) - 1 - 1) / 3,   // 32 bit
        ((uint128_t(1) << 64) - 1 - 1) / 3,   // 64 bit
        ((uint128_t(1) << 127) + ((uint128_t(1) << 127) - 1) - 1) / 3  // 128 bit
        // Requires a little juggling to avoid overflow.  Yay PEMDAS!
    };
    //
    // Now a helper for it.
    template<BuiltinIntegral T>
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

    // Precomputed maximum initial values for a given bit size.  The next value would overflow during its sequence.
    constexpr std::array<uint64_t, 65> MAX_INITIAL_VALUE_BY_64BIT = {
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
    };
    constexpr std::array<uint128_t, 129> MAX_INITIAL_VALUE_BY_128BIT = {
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

        // The following are placeholders until I can compute them.
        // This is the highest value I've computed without hitting the next 2^k max IV.
        "26453686357881703"_u128,  // 109
        "26453686357881703"_u128,  // 110
        "26453686357881703"_u128,  // 111
        "26453686357881703"_u128,  // 112
        "26453686357881703"_u128,  // 113
        "26453686357881703"_u128,  // 114
        "26453686357881703"_u128,  // 115
        "26453686357881703"_u128,  // 116
        "26453686357881703"_u128,  // 117
        "26453686357881703"_u128,  // 118
        "26453686357881703"_u128,  // 119
        "26453686357881703"_u128,  // 120
        "26453686357881703"_u128,  // 121
        "26453686357881703"_u128,  // 122
        "26453686357881703"_u128,  // 123
        "26453686357881703"_u128,  // 124
        "26453686357881703"_u128,  // 125
        "26453686357881703"_u128,  // 126
        "26453686357881703"_u128,  // 127
        "26453686357881703"_u128,  // 128
    };

    //
    // Max Check
    template<AnySupportedIntegral T>
    inline constexpr size_t get_max_initial_value_max_bits() {
        if constexpr(NativeIntegral<T>) {
            return MAX_INITIAL_VALUE_BY_64BIT.size() - 1;
        } else if constexpr(ExtendedIntegral<T> || GMPIntegral<T>) {
            return MAX_INITIAL_VALUE_BY_128BIT.size() - 1;
        }
    }
    //
    // Lookup
    template<AnySupportedIntegral T>
    inline constexpr T get_max_initial_value_by_bit(size_t bit_size) {
        // Safety Check
        if (bit_size > get_max_initial_value_max_bits<T>()) {
            throw std::out_of_range("Max initial value for bit size " + std::to_string(bit_size) + " not found.");
        }

        // Pick the right type.
        if constexpr(NativeIntegral<T>) {
            return MAX_INITIAL_VALUE_BY_64BIT[bit_size];
        } else if constexpr(ExtendedIntegral<T>) {
            return MAX_INITIAL_VALUE_BY_128BIT[bit_size];
        } else if constexpr(GMPIntegral<T>) {
            return uint128_to_mpz(MAX_INITIAL_VALUE_BY_128BIT[bit_size]);
        }
        throw std::logic_error("Unknown type for bit required.");
    }
}
