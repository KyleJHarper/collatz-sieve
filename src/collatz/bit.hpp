#pragma once
#include "concepts.hpp"
#include <gmp.h>
#include <limits>
#include <stdint.h>
#include <type_traits>




/// @brief Define `__has_builtin` in case it's missing.
#ifndef __has_builtin
  #define __has_builtin(x) 0
#endif



/**
* @brief Bit manipulation functions.
*
* Where bit reversal is desired, attempts to use the `__builtin_bitreverseX` family of functions.  Performance is highest when
* systems have them, but in their absence these methods will default to black-box-ish manual shifting, which is ironically not much
* slower
*
*/
namespace Bit {

    /// @brief Bit reversal for 8-bit unsigned type.  Uses `__builtin_bitreverse8()` if available, otherwise blackbox shifting.
    static inline constexpr uint8_t bitreverse8(uint8_t x) {
        #if __has_builtin(__builtin_bitreverse8)
        x = __builtin_bitreverse8(x);
        #else
        x = ((x >> 1) & 0x55) | ((x & 0x55) << 1);
        x = ((x >> 2) & 0x33) | ((x & 0x33) << 2);
        x = (x >> 4) | (x << 4);
        #endif
        return x;
    }



    /// @brief Bit reversal for 16-bit unsigned type.  Uses `__builtin_bitreverse16()` if available, otherwise blackbox shifting.
    static inline constexpr uint16_t bitreverse16(uint16_t x) {
        #if __has_builtin(__builtin_bitreverse16)
        x = __builtin_bitreverse16(x);
        #else
        x = ((x >> 1) & 0x5555) | ((x & 0x5555) << 1);
        x = ((x >> 2) & 0x3333) | ((x & 0x3333) << 2);
        x = ((x >> 4) & 0x0F0F) | ((x & 0x0F0F) << 4);
        x = (x >> 8) | (x << 8);
        #endif
        return x;
    }



    /// @brief Bit reversal for 32-bit unsigned type.  Uses `__builtin_bitreverse32()` if available, otherwise blackbox shifting.
    static inline constexpr uint32_t bitreverse32(uint32_t x) {
        #if __has_builtin(__builtin_bitreverse32)
        x = __builtin_bitreverse32(x);
        #else
        x = ((x >> 1)  & 0x55555555) | ((x & 0x55555555) << 1);
        x = ((x >> 2)  & 0x33333333) | ((x & 0x33333333) << 2);
        x = ((x >> 4)  & 0x0F0F0F0F) | ((x & 0x0F0F0F0F) << 4);
        x = ((x >> 8)  & 0x00FF00FF) | ((x & 0x00FF00FF) << 8);
        x = (x >> 16) | (x << 16);
        #endif
        return x;
    }



    /// @brief Bit reversal for 64-bit unsigned type.  Uses `__builtin_bitreverse64()` if available, otherwise blackbox shifting.
    static inline constexpr uint64_t bitreverse64(uint64_t x) {
        #if __has_builtin(__builtin_bitreverse64)
        x = __builtin_bitreverse64(x);
        #else
        x = ((x >> 1)  & 0x5555555555555555ULL) | ((x & 0x5555555555555555ULL) << 1);
        x = ((x >> 2)  & 0x3333333333333333ULL) | ((x & 0x3333333333333333ULL) << 2);
        x = ((x >> 4)  & 0x0F0F0F0F0F0F0F0FULL) | ((x & 0x0F0F0F0F0F0F0F0FULL) << 4);
        x = ((x >> 8)  & 0x00FF00FF00FF00FFULL) | ((x & 0x00FF00FF00FF00FFULL) << 8);
        x = ((x >> 16) & 0x0000FFFF0000FFFFULL) | ((x & 0x0000FFFF0000FFFFULL) << 16);
        x = (x >> 32) | (x << 32);
        #endif
        return x;
    }



    /**
    * @brief Reverse the bits of any type `T` requested.  Used largely for partial reversing later.
    *
    * The nature of the binary tree structure means certain liberties can be taken when calculating positions or node values within
    * the tree, largely due to the constant power-of-two increase.  To leverage this, one must take the bits from a node's value or
    * position and reverse them, but only a certain number of them on the LSB (least significant bit) side.  There are many ways to
    * do this, including a basic loop, which was used originally and then removed (diff 3.2.0).  Ultimately compiler intrisics were
    * and some black-box-ish helperes were built to speed this up since it's a hot path.
    *
    * @warning This method does not support `mpz_class`.  The methods which would use it handle `mpz_class` directly bit-by-bit. It
    * is entirely possible to handle `mpz_class` if needed some day, but it's expensive and unnecessary.
    *
    * @param x The data to reverse.
    * @tparam T A fixed-width integral.  (see concepts.hpp)
    * @return The same data and the same type, with bits reversed.
    */
    template<FixedWidthIntegral T>
    static inline T st_bit_reverse_full(const T& x) {
        // When the type is signed, convert it to unsigned and try again.
        if constexpr(std::numeric_limits<T>::is_signed) {
            using U = std::make_unsigned_t<T>;
            U ux = static_cast<U>(x);
            return st_bit_reverse_full(ux);
        }

        // Type is already unsigned.  Begin reversing.
        if constexpr (sizeof(T) == 1) {
            return static_cast<T>(bitreverse8(x));
        } else if constexpr (sizeof(T) == 2) {
            return static_cast<T>(bitreverse16(x));
        } else if constexpr (sizeof(T) == 4) {
            return static_cast<T>(bitreverse32(x));
        } else if constexpr (sizeof(T) == 8) {
            return static_cast<T>(bitreverse64(x));
        } else if constexpr (sizeof(T) == 16) {
            uint64_t low  = (uint64_t)x;
            uint64_t high = (uint64_t)(x >> 64);
            uint64_t rev_low  = bitreverse64(high);
            uint64_t rev_high = bitreverse64(low);
            return static_cast<T>(( (T)rev_high << 64 ) | rev_low);
        }
    }



    /**
    * @brief Reverse the lower bits of a value, returning only those reversed bits.
    *
    * @param value The data to undergo bit reversal.
    * @param bits The number of lower bits to reverse and return.
    * @tparam T Any supported integral (see concepts.hpp).
    * @return The lower bits isolated and reversed in a value of type `T`.
    */
    template<AnySupportedIntegral T>
    static inline T st_reverse_low_bits(const T& value, size_t bits) {
        // Test for zero.
        T result = 0;
        if (bits == 0) { return result; }

        // Reverse and shift by the correct amount.
        if constexpr(FixedWidthIntegral<T>) {
            result = st_bit_reverse_full(value);
            constexpr size_t WIDTH = sizeof(T) * 8;
            result >>= (WIDTH - bits);
        } else if constexpr(GMPIntegral<T>) {
            for (size_t bit = 0; bit < bits; bit++) {
                if (mpz_tstbit(value.get_mpz_t(), bit)) {
                    mpz_setbit(result.get_mpz_t(), bits - 1 - bit);
                }
            }
        }

        // Return.
        return result;
    }



    /**
    * @brief Reverse the lower bits of a value, returning only those reversed bits (out param version).
    *
    * @param value The data to undergo bit reversal.
    * @param bits The number of lower bits to reverse and return.
    * @param out The lower bits isolated and reversed in a value of type `T`.
    * @tparam T Any supported integral (see concepts.hpp).
    */
    template<AnySupportedIntegral T>
    static inline void st_reverse_low_bits(const T& value, size_t bits, T& out) {
        // Test for zero.
        out = 0;
        if (bits == 0) { return; }

        // Reverse and shift by the correct amount.
        if constexpr(FixedWidthIntegral<T>) {
            out = st_bit_reverse_full(value);
            constexpr size_t WIDTH = sizeof(T) * 8;
            out >>= (WIDTH - bits);
        } else if constexpr(GMPIntegral<T>) {
            for (size_t bit = 0; bit < bits; bit++) {
                if (mpz_tstbit(value.get_mpz_t(), bit)) {
                    mpz_setbit(out.get_mpz_t(), bits - 1 - bit);
                }
            }
        }
    }



    /**
    * @brief Find the number of trailing zeros for any type `T`.
    * @note When `x` is 0, this method returns 0.
    * @param x Reference to the value to apply CTZ on.
    * @tparam T Any supported integral (see concepts.hpp).
    * @return An integer telling how many were found.  This follows `__builtin_ctzll()` behavior.
    */
    template<AnySupportedIntegral T>
    inline int count_trailing_zeros(const T& x) {
        // Zero is a special and undefined case for builtins.  Return sizeof T in bits.
        if (x == 0) {
            return 0;
        }

        if constexpr(FixedWidthIntegral<T>) {
            // When it's below 64-bits (long long), we can use the builtin.
            if constexpr(sizeof(T) <= 8) {
                return __builtin_ctzll(x);
            }

            // Larger fixed-widths require one-off handling.
            if constexpr(Int128Integral<T>) {
                uint64_t low_bits = uint64_t(x);
                if (low_bits != 0) {
                    // There are ones in the lower bits.  Don't need high bits at all.
                    return __builtin_ctzll(low_bits);
                } else {
                    // There are no ones in the lower bits.  Return all 64 zeros plus whatever zeros are in high, if allowed.
                    uint64_t high_bits = uint64_t(x >> 64);
                    if (high_bits == 0) {
                        return 64;
                    } else {
                        return 64 + __builtin_ctzll(high_bits);
                    }
                }
            }

        } else if constexpr(GMPIntegral<T>) {
            // GMP has a scan1 function to find the first 1, so we can use it.
            return mpz_scan1(x.get_mpz_t(), 0);
        }

        // If execution ever reaches this, no valid path was found.
        throw std::logic_error("Cannot discern data type when calling count_trailing_zeros.");
    }



    /**
    * @brief Find the number of trailing ones for any type `T`.  Simply uses `count_trailing_zeros(~n)` essentially.
    * @param n Reference to the value to apply CTO on.
    * @tparam T Any supported integral (see concepts.hpp).
    * @return An integer telling how many were found.  This follows `__builtin_ctzll()` behavior.
    */
    template<AnySupportedIntegral T>
    inline int count_trailing_ones(const T& n) {
        // Trailing ones is equivalent to the trailing zeros of inverted n: ctz(~n).
        // GMP will perform an alloc for this, so we'll use TLS on that path.
        if constexpr(FixedWidthIntegral<T>) {
            T inverted_n = ~n;
            return count_trailing_zeros(inverted_n);
        } else if constexpr(GMPIntegral<T>) {
            static thread_local mpz_class inverted_n = 0;
            inverted_n = ~n;
            return count_trailing_zeros(inverted_n);
        }

        // If execution ever reaches this, no valid path was found.
        throw std::logic_error("Cannot discern data type when calling count_trailing_ones.");
    }



    /**
    * @brief Simple `bswap` helper.  Handles 8, 16, 32, 64, and 128 bit.
    * @param value Reference value to operate on.
    * @tparam T Any fixed-width integral (see concepts.hpp).
    * @return The bswap'd version, typed to matching `T`.
    */
    template<FixedWidthIntegral T>
    static T byteswap(T& value) {
        if constexpr (sizeof(T) == 1) {
            return value;
        } else if constexpr (sizeof(T) == 2) {
            return __builtin_bswap16(value);
        } else if constexpr (sizeof(T) == 4) {
            return __builtin_bswap32(value);
        } else if constexpr (sizeof(T) == 8) {
            return __builtin_bswap64(value);
        } else if constexpr (sizeof(T) == 16) {
            // 128-bit needs broken into two chunks.
            uint64_t high = static_cast<uint64_t>(value >> 64);
            uint64_t low  = static_cast<uint64_t>(value);
            high = __builtin_bswap64(high);
            low  = __builtin_bswap64(low);
            return (static_cast<T>(low) << 64) | high;
        } else {
            static_assert(sizeof(T) <= 16, "Unsupported size for byteswap");
            return value;
        }
    }

}
