#pragma once

#include <stdint.h>


/// @brief Make 128-bit types match naming pattern of the main concepts.hpp file.
typedef __uint128_t uint128_t;
typedef __int128_t int128_t;



/**
* @struct is_device_integral
* @brief Attempt to imitate AnySupportedIntegral for CUDA.
*/
template <typename T>
struct is_device_integral {
    static constexpr bool value = false;
};



/**
* @struct is_device_integral<uint32_t>
* @brief Attempt to imitate AnySupportedIntegral for CUDA.
*/
template <>
struct is_device_integral<uint32_t> {
    static constexpr bool value = true;
};



/**
* @struct is_device_integral<uint64_t>
* @brief Attempt to imitate AnySupportedIntegral for CUDA.
*/
template <>
struct is_device_integral<uint64_t> {
    static constexpr bool value = true;
};



/**
* @struct is_device_integral<uint128_t>
* @brief Attempt to imitate AnySupportedIntegral for CUDA.
*/
template <>
struct is_device_integral<uint128_t> {
    static constexpr bool value = true;
};



/// @brief is_device_integral_v to compare to all template overloaded variants.
template <typename T>
inline constexpr bool is_device_integral_v = is_device_integral<T>::value;
