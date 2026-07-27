#pragma once

#include "concepts.hpp"
#include <cxxabi.h>
#include <string>
#include <stdint.h>


/**
* @namespace ABI
* @brief Utilities to help interact with the Application Binary Interface (ABI).
*/
namespace ABI {

    /**
    * @brief Demangle's a mangled C++ type name.
    *
    * Converts a compiler-specific (and rather ugly) name returned by typeid() into a human-readable format.
    *
    * @param name The mangled string to be processed.
    * @return A string with the friendly name, or the original name if it can't be demangled.
    */
    inline std::string demangle(const char* name) {
        int status = 0;
        char* demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);
        std::string result = (status == 0 && demangled) ? demangled : name;
        free(demangled);
        return result;
    }



    /**
    * @brief Demange a mangled c++ type name based on the type sent.
    *
    * Converts a compiler-specific (and rather ugly) name returned by typeid() into a human-readable format.
    *
    * @tparam T Any type which responds to `typeid(T).name()`.
    * @return A string with the friendly name, or the original name if it can't be demangled.
    */
    template<typename T>
    inline std::string demangle() {
        return demangle(typeid(T).name());
    }



    /// @brief Use 64 bytes as the cache line size to avoid false sharing in TLS structs and similar.
    constexpr size_t CACHE_LINE_SIZE = 64;



    /// @brief Constant value for the mpz_class type ID.
    constexpr uint16_t MPZ_CLASS_BIT_WIDTH_ID = 0;



    /// @brief Assign consistent values to differnt data types.
    template<AnySupportedIntegral T>
    static constexpr uint16_t get_bit_width_id() {
        if constexpr(FixedWidthIntegral<T>) {
            return sizeof(T) * 8;
        } else if constexpr(GMPIntegral<T>) {
            return MPZ_CLASS_BIT_WIDTH_ID;
        }
    }
}
