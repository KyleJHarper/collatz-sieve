#pragma once

#include <concepts>
#include <gmp.h>
#include <gmpxx.h>



//
// We will use concepts to unify our template so it can support native integrals and GMP.
//
//
// Native Integrals
// These cover everything up to 64 bits, and are the fastest possible option.
//
// GMP
// Adds infinite bitsize at the cost of heap allocation.
template<typename T> struct is_mpz_class : std::false_type {};
template<> struct is_mpz_class<mpz_class> : std::true_type {};
//
// Generic concept for all types.
template<typename T>
concept IntegralOrMPZClass = std::integral<T> || is_mpz_class<T>::value;




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
