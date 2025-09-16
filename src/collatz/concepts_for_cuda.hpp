#pragma once

#include <stdint.h>


// Make 128-bit types match naming pattern of others.
typedef __uint128_t uint128_t;
typedef __int128_t int128_t;


//
// We can't actually use pure c++20 concepts, so we'll make a stripped-down version.
//
// Only allow uint32_t, uint64_t, and uint128_t in device code
template <typename T>
struct is_device_integral {
    static constexpr bool value = false;
};
// 32-bit
template <>
struct is_device_integral<uint32_t> {
    static constexpr bool value = true;
};
// 64-bit
template <>
struct is_device_integral<uint64_t> {
    static constexpr bool value = true;
};
// 128-bit
template <>
struct is_device_integral<uint128_t> {
    static constexpr bool value = true;
};
// Now the typed_v for it.
template <typename T>
inline constexpr bool is_device_integral_v = is_device_integral<T>::value;
