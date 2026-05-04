#pragma once

#include <concepts>
#include <stdexcept>
#include <stdint.h>
#include <gmp.h>
#include <gmpxx.h>
#include "abi.hpp"
#include "typedefs.hpp"
#include "int128.hpp"




/// @brief Concept for larger integrals at each bit size.  Currently only 128 bit.
template<typename T>
concept Int128Integral = (std::is_same_v<T, int128_t> || std::is_same_v<T, uint128_t>);



/// @brief Concept for any fixed-width integral.  Includes platform-specific widths such as `size_t` and `int`.
template<typename T>
concept FixedWidthIntegral = (std::integral<T> || Int128Integral<T>);



/// @brief Concept for GMP integrals: mpz_class.
template<typename T>
concept GMPIntegral = (std::is_same_v<T, mpz_class>);



/// @brief Any supported integral is either a `FixedWidthIntegral` or a `GMPIntegral`.
template<typename T>
concept AnySupportedIntegral = (FixedWidthIntegral<T> || GMPIntegral<T>);



/// @brief Concept applicable to any fixed, guaranteed-width type.  Mostly for serializing/deserializing.
template<typename T>
concept GuaranteedWidthIntegral = (
    std::is_same_v<T, int8_t> || std::is_same_v<T, uint8_t> ||
    std::is_same_v<T, int16_t> || std::is_same_v<T, uint16_t> ||
    std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> ||
    std::is_same_v<T, int64_t> || std::is_same_v<T, uint64_t> ||
    std::is_same_v<T, int128_t> || std::is_same_v<T, uint128_t>
);



/// @brief Concept to detect which integrals will respond to operator<< for printing.
template <typename T>
concept PrintableIntegral = std::integral<T> && requires(std::ostream& os, T val) {
    { os << val } -> std::same_as<std::ostream&>;
};



/**
* @brief Convert any supported integral `T` to a string.
* @param val The value to string-ify.
* @return A new string with the digits represented by the value.
* @tparam T Any supported integral (see concepts.hpp).
*/
template<AnySupportedIntegral T>
inline std::string to_string_any(const T& val) {
    if constexpr(PrintableIntegral<T>) {
        return std::to_string(val);
    } else if constexpr (Int128Integral<T>) {
        return Int128::uint128_to_string(val);
    } else if constexpr (GMPIntegral<T>) {
        return val.get_str();
    } else {
        throw std::runtime_error("Unknown type passed to to_string_any(): " + ABI::demangle<T>());
    }
}

