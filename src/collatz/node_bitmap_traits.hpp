#pragma once

#include <concepts>
#include <limits>
#include "concepts.hpp"




/// @brief Enum helper for letting callers decide between serial (sequential) and parallel operation.
enum class BitmapTransformerPolicy {
    SERIAL = 1,
    PARALLEL = 2,
};



/**
* @struct BitmapKeyTraits
* @brief Helpers to handle prefix and suffix keys for the hash map.
*/
template<AnySupportedIntegral T>
struct BitmapKeyTraits {
    /// @brief Suffix can be anything, potentially, but CRoaring recommends 32 bits.  Hard code that.
    using suffix_t = uint32_t;
    static_assert(sizeof(suffix_t) <= sizeof(unsigned long), "Suffix size (suffix_t) must not exceed mpz_t.get_ui(), which is unsigned long.");
    /// @brief Number of bytes in `suffix_t`.
    static constexpr size_t SUFFIX_BYTES = sizeof(suffix_t);
    /// @brief Number of bits in `suffix_t`.
    static constexpr size_t SUFFIX_BITS = sizeof(suffix_t) * 8;
    /// @brief The bitmask for `suffix_t`.
    static constexpr suffix_t SUFFIX_MASK = std::numeric_limits<suffix_t>::max();
    /// @brief Maximum value for `suffix_t`.
    static constexpr suffix_t SUFFIX_MAX = std::numeric_limits<suffix_t>::max();

    // Prefix is going to be fixed instead of a bunch of complicated logic.
    //   mpz_class -> mpz_class because only it can handle arbitrarily large numbers
    //   uint64_t or lower -> uint32_t because SUFFIX is hard-coded to 32 bits
    //   uint128_t -> uint128_t because we only need 96 bits but there's no uint96_t
    /**
    * @brief Prefix for a `FlatHashBitmapImpl`.
    *
    * Prefixes fall into these categories:
    *   1. When `T` is an `mpz_class`, prefix will be `mpz_class`.
    *   2. When `T` is less than or equal to 64 bits, prefix will be `uint32_t`.
    *   3. All others, prefix will be `uint128_t`.
    */
    using prefix_t = std::conditional_t<
        std::same_as<mpz_class, T>
        , mpz_class
        , std::conditional_t<
            (sizeof(T) <= 8)
            , uint32_t
            , uint128_t
        >
    >;


    // Roaring has internal types for its 32- and 64-bit versions, but doesn't expose a type for it.
    /// @brief CRoaring's key type size is 16 bits.
    using roaring_key_t = uint16_t;
    /// @brief CRoaring's value type size is 16 bits.
    using roaring_value_t = uint16_t;
    /// @brief CRoaring's typecode type size is 16 bits.
    using roaring_typecode_t = uint8_t;
    /// @brief CRoaring's word type size is 64 bits.
    using roaring_word_t = uint64_t;
    /// @brief CRoaring's bit size for keys (32-bit version).
    static constexpr size_t ROARING_KEY_BITS = 16;
    /// @brief CRoaring's bit size for values (32-bit version).
    static constexpr size_t ROARING_VALUE_BITS = 16;
    /// @brief CRoaring's bit size for words.
    static constexpr size_t ROARING_WORD_BITS = 64;



    /**
    * @brief Return the prefix for any type T.  When mpz_class, TLS.  See selection for prefix_t above.
    * @param value The value to shift to derive a prefix.
    * @return The prefix for `value`, as a `prefix_t` type.
    */
    static prefix_t get_prefix(const T& value) {
        if constexpr (BuiltinIntegral<T>) {
            if constexpr (sizeof(T) <= SUFFIX_BYTES) {
                return 0;
            } else {
                return static_cast<prefix_t>(value >> SUFFIX_BITS);
            }
        } else {
            static thread_local mpz_class tmp;
            mpz_fdiv_q_2exp(tmp.get_mpz_t(), value.get_mpz_t(), SUFFIX_BITS);
            return tmp;
        }
    }



    /**
    * @brief Return the prefix for any type T.  When mpz_class, TLS.  See selection for prefix_t above.
    * @param value The value to shift to derive a prefix.
    * @param out Reference to a `prefix_t` where the prefix will be saved.
    */
    static void get_prefix(const T& value, prefix_t& out) {
        if constexpr (BuiltinIntegral<T>) {
            if constexpr (sizeof(T) <= SUFFIX_BYTES) {
                out = 0;
            } else {
                out = static_cast<prefix_t>(value >> SUFFIX_BITS);
            }
        } else {
            mpz_fdiv_q_2exp(out.get_mpz_t(), value.get_mpz_t(), SUFFIX_BITS);
        }
    }



    /**
    * @brief Return the suffix for any type `T`.
    * @param value The value to mask to derive a suffix.
    * @return The suffix for `value`, as a `suffix_t` type.
    */
    static suffix_t get_suffix(const T& value) {
        if constexpr (BuiltinIntegral<T>) {
            return static_cast<suffix_t>(value & SUFFIX_MASK);
        } else {
            return static_cast<suffix_t>(value.get_ui());
        }
    }

};
