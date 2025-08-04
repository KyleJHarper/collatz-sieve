#pragma once

#include <cstddef>
#include <gmpxx.h>

template<typename T>
inline size_t gmp_deep_sizeof(const T& obj) {
    return sizeof(T);
}

template<>
inline size_t gmp_deep_sizeof<mpz_class>(const mpz_class& val) {
    return sizeof(mpz_class) + (mpz_size(val.get_mpz_t()) * sizeof(mp_limb_t));
}

template<>
inline size_t gmp_deep_sizeof<mpf_class>(const mpf_class& val) {
    return sizeof(mpf_class) + (mpf_size(val.get_mpf_t()) * sizeof(mp_limb_t));
}
