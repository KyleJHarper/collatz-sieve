#pragma once
#include <stdexcept>
#include "abi.hpp"
#include "int128.hpp"
#include "concepts.hpp"


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

