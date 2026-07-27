#pragma once

#include <concepts>
#include <stdint.h>
#include <gmp.h>
#include <gmpxx.h>
#include "typedefs.hpp"




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



/// @brief Concept to detect which integrals will respond to operator<< for printing.
template<typename T>
concept PrintableIntegral = std::integral<T> && requires(std::ostream& os, T val) {
    { os << val } -> std::same_as<std::ostream&>;
};
