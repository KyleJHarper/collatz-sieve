#pragma once
#include "int128.hpp"
#include "concepts.hpp"


/**
* @brief Convert any supported integral `T` to a string.
* @param val The value to string-ify.
* @param separators Whether to emit a thousands separator (regardless of Locale).
* @return A new string with the digits represented by the value.
* @tparam T Any supported integral (see concepts.hpp).
*/
template<AnySupportedIntegral T>
inline std::string to_string_any(const T& val, bool separators = false) {
    std::string result;
    if constexpr(PrintableIntegral<T>) {
        result = std::to_string(val);
    } else if constexpr (Int128Integral<T>) {
        result = Int128::uint128_to_string(val);
    } else if constexpr (GMPIntegral<T>) {
        result = val.get_str();
    }

    if (separators) {
        for (int i = static_cast<int>(result.size()) - 3; i > 0; i -= 3) {
            result.insert(i, ",");
        }
    }

    return result;
}
