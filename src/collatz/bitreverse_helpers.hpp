#pragma once
#include <stdint.h>


//
// Only clang has the __builtin_bitreverseX family.  We'll roll our own by including some blackbox functions and inline them.
//
// We will use the built-in if it's available according to __has_builtin(), and if that's missing too, fallback to manual shifts.
//


//
// In case the __has_builtin is gone.
//
#ifndef __has_builtin
  #define __has_builtin(x) 0
#endif



//
// 8-bit
//
static inline constexpr uint8_t bitreverse8(uint8_t x) {
    #if __has_builtin(__builtin_bitreverse8)
    x = __builtin_bitreverse8(x);
    #else
    x = ((x >> 1) & 0x55) | ((x & 0x55) << 1);
    x = ((x >> 2) & 0x33) | ((x & 0x33) << 2);
    x = (x >> 4) | (x << 4);
    #endif
    return x;
}



//
// 16-bit
//
static inline constexpr uint16_t bitreverse16(uint16_t x) {
    #if __has_builtin(__builtin_bitreverse16)
    x = __builtin_bitreverse16(x);
    #else
    x = ((x >> 1) & 0x5555) | ((x & 0x5555) << 1);
    x = ((x >> 2) & 0x3333) | ((x & 0x3333) << 2);
    x = ((x >> 4) & 0x0F0F) | ((x & 0x0F0F) << 4);
    x = (x >> 8) | (x << 8);
    #endif
    return x;
}



//
// 32-bit
//
static inline constexpr uint32_t bitreverse32(uint32_t x) {
    #if __has_builtin(__builtin_bitreverse32)
    x = __builtin_bitreverse32(x);
    #else
    x = ((x >> 1)  & 0x55555555) | ((x & 0x55555555) << 1);
    x = ((x >> 2)  & 0x33333333) | ((x & 0x33333333) << 2);
    x = ((x >> 4)  & 0x0F0F0F0F) | ((x & 0x0F0F0F0F) << 4);
    x = ((x >> 8)  & 0x00FF00FF) | ((x & 0x00FF00FF) << 8);
    x = (x >> 16) | (x << 16);
    #endif
    return x;
}



//
// 64-bit
//
static inline constexpr uint64_t bitreverse64(uint64_t x) {
    #if __has_builtin(__builtin_bitreverse64)
    x = __builtin_bitreverse64(x);
    #else
    x = ((x >> 1)  & 0x5555555555555555ULL) | ((x & 0x5555555555555555ULL) << 1);
    x = ((x >> 2)  & 0x3333333333333333ULL) | ((x & 0x3333333333333333ULL) << 2);
    x = ((x >> 4)  & 0x0F0F0F0F0F0F0F0FULL) | ((x & 0x0F0F0F0F0F0F0F0FULL) << 4);
    x = ((x >> 8)  & 0x00FF00FF00FF00FFULL) | ((x & 0x00FF00FF00FF00FFULL) << 8);
    x = ((x >> 16) & 0x0000FFFF0000FFFFULL) | ((x & 0x0000FFFF0000FFFFULL) << 16);
    x = (x >> 32) | (x << 32);
    #endif
    return x;
}
