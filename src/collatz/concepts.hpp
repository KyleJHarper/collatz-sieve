#ifndef SRC_CONCEPTS_H_
#define SRC_CONCEPTS_H_

#include <concepts>
#include <gmpxx.h>

//
// We will use concepts to unify our template so it can support integrals and GMP.
//
template<typename T>
concept IntegralOrMPZClass = std::integral<T> || std::same_as<T, mpz_class>;

#endif
