#pragma once
#include <string_view>
#include <gmpxx.h>
#include "typedefs.hpp"
// #include "concepts.hpp"



/**
* @namespace Int128
* @brief Collection of methods for managing 128-bit integer data types.
*/
namespace Int128 {

    /**
    * @brief Convert a string view to a `uint128_t`.
    * @param view The string view to parse.
    * @return The uint128_t representation of the string digits sent, if valid.
    */
    constexpr uint128_t parse_u128(std::string_view view) {
        uint128_t value = 0;
        for (char c : view) {
            if (c < '0' || c > '9')
                throw "Invalid digit in uint128 literal";
            value = value * 10 + (c - '0');
        }
        return value;
    }



    /**
    * @brief Convert a `uint128_t` type to an `mpz_class`, storing it in caller's memory.
    * @param v Value to convert.
    * @param dest Reference to the `mpz_class` object to write to.
    */
    inline void uint128_to_mpz(uint128_t v, mpz_class& dest) {
        uint64_t parts[2] = {
            static_cast<uint64_t>(v),       // low 64
            static_cast<uint64_t>(v >> 64)  // high 64
        };
        mpz_import(dest.get_mpz_t(), 2, -1, sizeof(parts[0]), 0, 0, parts);
    }



    /**
    * @brief Convert a `uint128_t` to an `mpz_class`, returning new memory.
    * @param v Value to convert.
    * @return The same value in an `mpz_class` object.
    */
    inline mpz_class uint128_to_mpz(uint128_t v) {
        mpz_class result;
        uint128_to_mpz(v, result);
        return result;
    }



    /**
    * @brief Convert a `uint128_t` to an `mpf_class`, returning new memory.
    * @param v Value to convert.
    * @return The same value in an `mpf_class` object.
    */
    inline mpf_class uint128_to_mpf(uint128_t v) {
        return mpf_class(uint128_to_mpz(v));
    }



    /**
    * @brief Convert a signed `int128_t` value to string.
    * @param value Value to string-ify.
    * @return New string with the digits represented by the value, including a negative sign "-".
    */
    inline std::string int128_to_string(int128_t value) {
        if (value == 0) return "0";

        bool negative = value < 0;
        uint128_t u_value = negative ? uint128_t(-(value + 1)) + 1  : uint128_t(value);

        std::string result;
        while (u_value > 0) {
            auto digit = static_cast<unsigned>(u_value % 10);
            result.push_back(static_cast<char>('0' + digit));
            u_value /= 10;
        }
        if (negative) result.push_back('-');
        std::reverse(result.begin(), result.end());
        return result;
    }



    /**
    * @brief Convert an unsigned `uint128_t` value to string.
    * @param value Value to string-ify.
    * @return New string with the digits represented by the value.
    */
    inline std::string uint128_to_string(uint128_t value) {
        if (value == 0) return "0";

        std::string result;
        while (value > 0) {
            auto digit = static_cast<unsigned>(value % 10);
            result.push_back(static_cast<char>('0' + digit));
            value /= 10;
        }
        std::reverse(result.begin(), result.end());
        return result;
    }



    /**
    * @brief Convert a string to a `uint128_t`.
    * @param s The string to convert.
    * @return A `uint128_t` representing the digits converted to an integral.
    */
    inline uint128_t str_to_uint128(const std::string& s) {
        if (s.empty()) {
            throw std::invalid_argument("Empty string for uint128_t");
        }
        uint128_t result = 0;
        for (char c : s) {
            if (!std::isdigit(c)) {
                throw std::invalid_argument("Invalid character in uint128_t: " + s);
            }
            int digit = c - '0';
            result = result * 10 + digit;
        }
        return result;
    }

}
