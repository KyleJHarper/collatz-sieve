#pragma once

#include <concepts>
#include <stdint.h>
#include <string_view>
#include <gmp.h>
#include <gmpxx.h>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>



//
// This file is a bit of a catch-all "internal.h"-style header.  We will define concepts, typedef things for convenience, and make
// a handful of helpers for things like UDLs, uint128_t/mpz_class parsing, and string-ifying things.
//




// Make 128-bit types match naming pattern of others.
typedef __uint128_t uint128_t;
typedef __int128_t int128_t;

// Level type should be guaranteed across platforms, not size_t.
typedef uint32_t level_t;

// Sequences are not size_t in length.  Use a common type that's smaller.
typedef uint32_t seq_size_t;



/**
* @brief Convert a string view to a `uint128_t`.
* @param view The string view to parse.
* @return The uint128_t representation of the string digits sent, if valid.
*/
constexpr uint128_t parse_u128(std::string_view view) {
    uint128_t value = 0;
    for (char c : view) {
        if (c < '0' || c > '9')
            throw "invalid digit in _u128 literal";
        value = value * 10 + (c - '0');
    }
    return value;
}



/**
* @brief A custom UDL to convert any string into a `uint128_t`, including at compile time.
* @param str The string containing digits.
* @param len The length of the string, which is automatic if done at compile time.
*/
constexpr uint128_t operator""_u128(const char* str, size_t len) {
    return parse_u128(std::string_view{str, len});
}



/**
* @brief Convert a string view to an `mpz_class`.  Uses GMP's `mpz_set_string()` internally.
* @param view The string view to parse.
* @return The mpz_class representation of the string digits sent, if valid.
*/
inline mpz_class parse_mpz(std::string_view view) {
    mpz_class value;
    if (mpz_set_str(value.get_mpz_t(), view.data(), 10) != 0) {
        throw std::runtime_error("invalid digit in _mpz literal");
    }
    return value;
}



/**
* @brief A custom UDL to convert any string into a `mpz_class`.
* @param str The string containing digits.
* @param len The length of the string, which is automatic if done at compile time.
*/
inline mpz_class operator""_mpz(const char* str, size_t len) {
    return parse_mpz(std::string_view{str, len});
}



/**
* @struct is_native_integral
* @brief Container for true if `T` is a "native" integral to the CPU (<= 64 bits).
*/
template<typename T>
struct is_native_integral : std::bool_constant<std::integral<T> && ! (std::same_as<T, uint128_t> || std::same_as<T, int128_t>)> {};

/// @brief Constexpr for any `T` for a native integral via `is_native_integral` struct.
template<typename T>
inline constexpr bool is_native_integral_v = is_native_integral<T>::value;

/// @brief Concept applicable to `T` when it's a native integral.
template<typename T>
concept NativeIntegral = is_native_integral_v<T>;



/**
* @struct is_extended_integral
* @brief Container for true if `T` is an "extended" integral of the compiler (i.e.: 128-bit).
*/
template<typename T>
struct is_extended_integral : std::bool_constant<std::same_as<T, uint128_t> || std::same_as<T, int128_t>> {};

/// @brief Constexpr for any `T` for an extended integral via `is_extended_integral` struct.
template<typename T>
inline constexpr bool is_extended_integral_v = is_extended_integral<T>::value;

/// @brief Concept applicable to `T` when it's an extended integral.
template<typename T>
concept ExtendedIntegral = is_extended_integral_v<T>;



/**
* @struct is_builtin_integral
* @brief Container for true if `T` is a "builtin" integral of the CPU or compiler (<= 128 bits).
*/
template<typename T>
struct is_builtin_integral : std::bool_constant<std::integral<T> || std::same_as<T, uint128_t> || std::same_as<T, int128_t>> {};

/// @brief Constexpr for any `T` for a builtin integral via `is_builtin_integral` struct.
template<typename T>
inline constexpr bool is_builtin_integral_v = is_builtin_integral<T>::value;

/// @brief Concept applicable to `T` when it's a builtin integral.
template<typename T>
concept BuiltinIntegral = is_builtin_integral_v<T>;



/**
* @struct is_mpz_class
* @brief Container for true if `T` is an `mpz_class` from GMP.
*/
template<typename T> struct is_mpz_class : std::false_type {};
template<> struct is_mpz_class<mpz_class> : std::true_type {};

/// @brief Constexpr for any `T` for an mpz_class via `is_mpz_class` struct.
template<typename T>
inline constexpr bool is_mpz_class_v = is_mpz_class<T>::value;



/**
* @struct is_gmp_integral
* @brief Container for true if `T` is an `mpz_class` from GMP.
*/
template<typename T>
struct is_gmp_integral : is_mpz_class<T> {};

/// @brief Constexpr for any `T` for a GMP integral via `is_gmp_integral` struct.
template<typename T>
inline constexpr bool is_gmp_integral_v = is_gmp_integral<T>::value;

/// @brief Concept applicable to `T` when it's a GMP integral.
template<typename T>
concept GMPIntegral = is_gmp_integral_v<T>;



/**
* @struct is_any_supported_integral
* @brief Container for true if `T` matches any of the supported types for this API.
*/
template<typename T>
struct is_any_supported_integral : std::bool_constant<is_native_integral_v<T> || is_extended_integral_v<T> || is_gmp_integral_v<T>> {};

/// @brief Constexpr for any `T` for any supported integral via `is_any_supported_integral` struct.
template<typename T>
inline constexpr bool is_any_supported_integral_v = is_any_supported_integral<T>::value;

/// @brief Concept applicable to `T` when it's any supported integral.
template<typename T>
concept AnySupportedIntegral = is_any_supported_integral_v<T>;



/**
* @brief Concept applicable to any fixed, guaranteed-width type.  Mostly for serializing/deserializing.
*/
template<typename T>
concept GuaranteedWidthIntegral = (
    std::is_same_v<T, int8_t> || std::is_same_v<T, uint8_t> ||
    std::is_same_v<T, int16_t> || std::is_same_v<T, uint16_t> ||
    std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> ||
    std::is_same_v<T, int64_t> || std::is_same_v<T, uint64_t> ||
    std::is_same_v<T, int128_t> || std::is_same_v<T, uint128_t>
);



//
// Create some convenient same_as style concepts for checking.
//
// Vector
template<typename T>
struct is_vector : std::false_type {};
template<typename T, typename Alloc>
struct is_vector<std::vector<T, Alloc>> : std::true_type {};
template<typename T>
concept Vector = is_vector<std::remove_cvref_t<T>>::value;


// Map
template<typename T>
struct is_map : std::false_type {};
template<typename K, typename V, typename Comp, typename Alloc>
struct is_map<std::map<K, V, Comp, Alloc>> : std::true_type {};
template<typename T>
concept Map = is_map<std::remove_cvref_t<T>>::value;




// Unordered Map
template<typename T>
struct is_unordered_map : std::false_type {};
template<typename K, typename V, typename Hash, typename Eq, typename Alloc>
struct is_unordered_map<std::unordered_map<K, V, Hash, Eq, Alloc>> : std::true_type {};
template<typename T>
concept UnorderedMap = is_unordered_map<std::remove_cvref_t<T>>::value;


// Set
template<typename T>
struct is_set : std::false_type {};
template<typename K, typename Comp, typename Alloc>
struct is_set<std::set<K, Comp, Alloc>> : std::true_type {};
template<typename T>
concept Set = is_set<std::remove_cvref_t<T>>::value;



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
    unsigned __int128 v = negative ? -value : value;

    std::string result;
    while (v > 0) {
        int digit = v % 10;
        result.push_back('0' + digit);
        v /= 10;
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
    std::string s;
    while (value > 0) {
        unsigned digit = value % 10;
        s.push_back('0' + digit);
        value /= 10;
    }
    std::reverse(s.begin(), s.end());
    return s;
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



/**
* @brief Convert any supported integral `T` to a string.
* @param val The value to string-ify.
* @return A new string with the digits represented by the value.
* @tparam T Any supported integral (see concepts.hpp).
*/
template<AnySupportedIntegral T>
inline std::string to_string_any(const T& val) {
    if constexpr(NativeIntegral<T>) {
        return std::to_string(val);
    } else if constexpr (ExtendedIntegral<T>) {
        return uint128_to_string(val);
    } else if constexpr (GMPIntegral<T>) {
        return val.get_str();
    }
}




//
// Help casting things to unsigned, mostly in bit reversal.
template<typename T>
struct make_unsigned128_helper {
    using type = std::make_unsigned_t<T>;
};

// Specialization for signed __int128
template<>
struct make_unsigned128_helper<uint128_t> {
    using type = unsigned __int128;
};

// Helper alias
template<typename T>
using make_unsigned_custom_t = typename make_unsigned128_helper<T>::type;



//
// Absl hashing helpers for MPZ types.
//
/**
* @brief Add capability to has `mpz_class` types with Abseil-CPP.
* @tparam H Used internally by Absl.
* @param value The value to hash, which happens by combining the limb count and data to `h`.
*/
template <typename H>
H AbslHashValue(H h, const mpz_class& value) {
    // Get the limb count and add it to the hash.
    const mp_limb_t* limbs = mpz_limbs_read(value.get_mpz_t());
    size_t limb_count = mpz_size(value.get_mpz_t());
    h = H::combine(std::move(h), limb_count);

    // Now include the limbs themselves.
    for (size_t i = 0; i < limb_count; i++) {
        h = H::combine(std::move(h), limbs[i]);
    }
    return h;
}



/**
* @struct MpzEq
* @brief An equality operator wrapped in a struct for Abseil-CPP to compare hash values with.
*/
struct MpzEq {
    bool operator()(const mpz_class& a, const mpz_class& b) const {
        return a == b;
    }
};



/// @brief Helper to tell if the system is using little endian.
constexpr bool is_little_endian() {
    return std::endian::native == std::endian::little;
}



/// @brief Helper to tell if the system is using big endian.
constexpr bool is_big_endian() {
    return std::endian::native == std::endian::big;
}
