#pragma once
#include "concepts.hpp"



// Helper to count trailing zeros.
template<AnySupportedIntegral T>
inline int count_trailing_zeros(const T& n) {
    if (n == 0) {
        return 0;
    }

    if constexpr(NativeIntegral<T>) {
        // Native integrals can use the builtin counter as-is.
        return __builtin_ctzll(n);
    } else if constexpr(ExtendedIntegral<T>) {
        // Extended (128-bit) integrals can check the low and high bits and return accordingly.
        uint64_t low_bits = uint64_t(n);
        if (low_bits != 0) {
            // There are ones in the lower bits.  Don't need high bits at all.
            return __builtin_ctzll(low_bits);
        } else {
            uint64_t high_bits = uint64_t(n >> 64);
            // There are no zeros in the lower bits.  Return all 64 of them plus whatever is in high.
            return 64 + __builtin_ctzll(high_bits);
        }
    } else if constexpr(GMPIntegral<T>) {
        // GMP has a scan1 function to find the first 1, so we can use it.
        return mpz_scan1(n.get_mpz_t(), 0);
    } else {
        throw std::logic_error("Cannot discern data type when calling count_trailing_zeros.");
    }
}



// Helper to count trailing ones.
template<AnySupportedIntegral T>
inline int count_trailing_ones(const T& n) {
    if (n == 0) {
        return 0;
    }

    if constexpr(NativeIntegral<T>) {
        // Native integrals can use the builtin counter as-is, just check for all 1s which breaks CTZ.
        T inverted_n = ~n;
        if (inverted_n == 0) {
            return std::numeric_limits<T>::digits;
        }
        return __builtin_ctzll(inverted_n);
    } else if constexpr(ExtendedIntegral<T>) {
        // Extended (128-bit) integrals can check the low and high bits and return accordingly.
        T inverted_n = ~n;
        uint64_t low_bits = uint64_t(inverted_n);
        if (low_bits != 0) {
            // There are ones in the inverted lower bits (zeros in original n).  Don't need high bits at all.
            return __builtin_ctzll(low_bits);
        } else {
            uint64_t high_bits = uint64_t(inverted_n >> 64);
            // There are no zeros in the lower bits.  Return all 64 of them plus whatever is in high.
            return 64 + __builtin_ctzll(high_bits);
        }
    } else if constexpr(GMPIntegral<T>) {
        // GMP has a scan1 function to find the first 1, so we can use it.
        static thread_local mpz_class inverted_n = 0;
        inverted_n = ~n;
        return mpz_scan1(inverted_n.get_mpz_t(), 0);
    } else {
        throw std::logic_error("Cannot discern data type when calling count_trailing_ones.");
    }
}
