#pragma once

#include <concepts>
#include <stdint.h>
#include <gmp.h>
#include <gmpxx.h>


// Make 128-bit types match naming pattern of others.
typedef __uint128_t uint128_t;
typedef __int128_t int128_t;


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
