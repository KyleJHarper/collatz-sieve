#pragma once

#include <concepts>
#include <stdint.h>
#include <string_view>
#include <gmp.h>
#include <gmpxx.h>
#include <absl/hash/hash.h>



// Make 128-bit types match naming pattern of others.
typedef __uint128_t uint128_t;
typedef __int128_t int128_t;




//
// Make a UDL for str-to-uint128 since we can't trust the compiler to understand it.
//
// Helper method.
constexpr uint128_t parse_u128(std::string_view s) {
    uint128_t value = 0;
    for (char c : s) {
        if (c < '0' || c > '9')
            throw "invalid digit in _u128 literal";
        value = value * 10 + (c - '0');
    }
    return value;
}
//
// Here's the UDL.
constexpr uint128_t operator""_u128(const char* str, size_t len) {
    return parse_u128(std::string_view{str, len});
}



//
// Now make a UDL for MPZ class
//
inline mpz_class parse_mpz(std::string_view s) {
    mpz_class value;
    if (mpz_set_str(value.get_mpz_t(), s.data(), 10) != 0) {
        throw std::runtime_error("invalid digit in _mpz literal");
    }
    return value;
}
//
// Here's the UDL.
inline mpz_class operator""_mpz(const char* str, size_t len) {
    return parse_mpz(std::string_view{str, len});
}


//
// We will use concepts to unify our template so it can support native integrals and GMP.
//
//
// Native Integrals
// These cover everything up to 64 bits, and are the fastest possible option.
template<typename T>
struct is_native_integral : std::bool_constant<std::integral<T> && ! (std::same_as<T, uint128_t> || std::same_as<T, int128_t>)> {};
template<typename T>
inline constexpr bool is_native_integral_v = is_native_integral<T>::value;
template<typename T>
concept NativeIntegral = is_native_integral_v<T>;
//
// Extended Integrals
// These give 128-bit support without GMP overhead.
template<typename T>
struct is_extended_integral : std::bool_constant<std::same_as<T, uint128_t> || std::same_as<T, int128_t>> {};
template<typename T>
inline constexpr bool is_extended_integral_v = is_extended_integral<T>::value;
template<typename T>
concept ExtendedIntegral = is_extended_integral_v<T>;
//
// Built-In (Native + Extended)
template<typename T>
struct is_builtin_integral : std::bool_constant<std::integral<T> || std::same_as<T, uint128_t> || std::same_as<T, int128_t>> {};
template<typename T>
inline constexpr bool is_builtin_integral_v = is_builtin_integral<T>::value;
template<typename T>
concept BuiltinIntegral = is_builtin_integral_v<T>;
//
// GMP
// Adds infinite bitsize at the cost of heap allocation.
template<typename T> struct is_mpz_class : std::false_type {};
template<> struct is_mpz_class<mpz_class> : std::true_type {};
template<typename T>
inline constexpr bool is_mpz_class_v = is_mpz_class<T>::value;
template<typename T>
struct is_gmp_integral : is_mpz_class<T> {};
template<typename T>
inline constexpr bool is_gmp_integral_v = is_gmp_integral<T>::value;
template<typename T>
concept GMPIntegral = is_gmp_integral_v<T>;
//
// Any Type
template<typename T>
struct is_any_supported_integral : std::bool_constant<is_native_integral_v<T> || is_extended_integral_v<T> || is_gmp_integral_v<T>> {};
template<typename T>
inline constexpr bool is_any_supported_integral_v = is_any_supported_integral<T>::value;
template<typename T>
concept AnySupportedIntegral = is_any_supported_integral_v<T>;




//
//  Helpers to convert uint128_t to GMP types.
//
inline void uint128_to_mpz(uint128_t v, mpz_class& dest) {
    uint64_t parts[2] = {
        static_cast<uint64_t>(v),       // low 64
        static_cast<uint64_t>(v >> 64)  // high 64
    };
    mpz_import(dest.get_mpz_t(), 2, -1, sizeof(parts[0]), 0, 0, parts);
}
//
// Overload to send a copy instead.
inline mpz_class uint128_to_mpz(uint128_t v) {
    mpz_class result;
    uint128_to_mpz(v, result);
    return result;
}
//
// Overload to modify an mpz_class directly to avoid alloc.
//
// Get an MPF instead.
inline mpf_class uint128_to_mpf(uint128_t v) {
    return mpf_class(uint128_to_mpz(v));
}




//
// String Helper
//
// Generic to_string that works with uint128_t, GMP, and native integrals
inline std::string int128_to_string(__int128_t value) {
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
// We only allow const refs in our callback methods in for_each... stuff.
// This is a little bit of black-magic from online resources, but it works.
//
template<typename F>
struct first_arg_type;
//
// For function pointers
template<typename R, typename Arg, typename... Args>
struct first_arg_type<R(*)(Arg, Args...)> {
    using type = Arg;
};
//
// For member function pointers
template<typename R, typename C, typename Arg, typename... Args>
struct first_arg_type<R(C::*)(Arg, Args...) const> {
    using type = Arg;
};
//
// For generic callables (lambdas, functors)
template<typename F>
struct first_arg_type {
    private:
    // We'll try to use operator() signature:
    template<typename U>
    static auto test(int) -> decltype(&U::operator());

    template<typename U>
    static void test(...);

    using op_type = decltype(test<F>(0));
    public:
    using type = typename first_arg_type<op_type>::type;
};




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
//
// An "equals" operator in a struct.
struct MpzEq {
    bool operator()(const mpz_class& a, const mpz_class& b) const {
        return a == b;
    }
};