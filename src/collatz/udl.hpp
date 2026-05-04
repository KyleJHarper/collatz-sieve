#pragma once
#include "gmp.hpp"
#include "int128.hpp"




/**
* @brief A custom UDL to convert any string into a `mpz_class`.
* @param str The string containing digits.
* @param len The length of the string, which is automatic if done at compile time.
*/
inline mpz_class operator""_mpz(const char* str, size_t len) {
    return GMP::parse_mpz(std::string_view{str, len});
}



/**
* @brief A custom UDL to convert any string into a `uint128_t`, including at compile time.
* @param str The string containing digits.
* @param len The length of the string, which is automatic if done at compile time.
*/
constexpr uint128_t operator""_u128(const char* str, size_t len) {
    return Int128::parse_u128(std::string_view{str, len});
}
