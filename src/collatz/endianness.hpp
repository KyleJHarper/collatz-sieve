#pragma once
#include <bit>



/**
* @namespace Endian
* @brief Just a few helpers for endian behavior testing.
*/
namespace Endian {

    /// @brief Helper to tell if the system is using little endian.
    constexpr bool is_little_endian() {
        return std::endian::native == std::endian::little;
    }



    /// @brief Helper to tell if the system is using big endian.
    constexpr bool is_big_endian() {
        return std::endian::native == std::endian::big;
    }

}
