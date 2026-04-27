#pragma once
#include <concepts>
#include "concepts.hpp"
#include <stdint.h>
#include <stddef.h>
#include <array>



/**
* @namespace AffineStride
* @brief A multi-step (stride) of accelerated Collatz functions `F` and `G` stored as an affine map.
*
* Since affine maps remain affine under composition, "striding" maps can be built for any N whose least significant bits match the
* composition which created the map.  When thrown into a table, a process can stride "k" steps at a time with a single application
* of the accelerated map.
*/
namespace AffineStride {
    /// @brief Maximum stride length for the standard `Stride`.
    constexpr size_t MAX_STRIDE = 20;
    /// @brief A lightweight, data-only struct for the stride factors.
    struct Stride {
        uint32_t multiply = 1;  ///< Peak is all F's, which is ceil(log2(3^20)) == 32 bits.
        uint16_t add = 0;       ///< Peak is all F's, which is ceil(log2(~3324)) == 12 bits.
        uint8_t f_steps = 0;    ///< Can hold 255 steps.  More than enough.
        uint8_t shift = 0;      ///< Can hold 255 steps.  More than enough.
    };
    static_assert(sizeof(Stride) == 8);



    /// @brief Maximum stride length for the `LongStride`.
    constexpr size_t MAX_LONG_STRIDE = 40;
    /// @brief A heavier, data-only struct for longer strides.
    struct LongStride {
        uint64_t multiply = 1;  ///< Peak is all F's, which is ceil(log2(3^40)) == 64 bits.
        uint32_t add = 0;       ///< Peak is all F's, which is ceil(log2(~11057331) ==24 bits.
        uint8_t f_steps = 0;    ///< Can hold 255 steps.  More than enough.
        uint8_t shift = 0;      ///< Can hold 255 steps.  More than enough.
    };



    /**
    * @brief Build a stride table at compile-time.  Used by `STRIDE_TABLE`.
    * @tparam StrideType The type of stride struct to use.
    * @tparam bits The number of bits (steps) to build the table.  Final size is 2^bits.
    * @return A constexpr array of `Stride` or `LongStride` covering all starting bit patterns.
    */
    template<typename StrideType, size_t bits>
    constexpr std::array<StrideType, 1ULL << bits> build_stride_table() {
        if constexpr (std::same_as<StrideType, Stride>) {
            static_assert(bits <= MAX_STRIDE, "Stride bits exceed MAX_STRIDE");
        } else if constexpr (std::same_as<StrideType, LongStride>) {
            static_assert(bits <= MAX_LONG_STRIDE, "Stride bits exceed MAX_LONG_STRIDE");
        }

        // Build table.
        const uint64_t table_size = uint64_t(1) << bits;
        std::array<StrideType, 1ULL << bits> stride_table{};

        // With our table preallocated, we can use index-based iteration.  Since we're looking at LSBs, we naturally get our order
        // and can simply loop through them sequentially.
        for (uint64_t i = 0; i < table_size; i++) {
            uint64_t n = i;
            StrideType stride;
            for (size_t step_count = 0; step_count < bits; step_count++) {
                if (n & 1) {
                    stride.add = ((stride.add << 1) + stride.add) + (uint64_t(1) << stride.shift);
                    stride.multiply *= 3;
                    stride.shift++;
                    stride.f_steps++;
                    // Mimic the effects on n now.
                    n = (3 * n + 1) >> 1;
                } else {
                    stride.shift++;
                    // Mimic the effects on n now.
                    n >>= 1;
                }
            }
            stride_table[i] = stride;
        }

        return stride_table;
    }


    // Set stride size and build a mask and table using it.
    // Micro-testing shows 8 (256 permutations) is ideal.  Larger seems to spill out of cache (L1 I'd guess).  Smaller is worse.
    // These are constexpr, which explodes the compiler at a given limit, usually ~16. FYI.
    /// @brief Size of steps in each stride for the global lookup table in this namespace.
    /// @note Micro-benchmarking shows 8 (256 permutations) is ideal.  Larger seems to spill out of L1 cache, possibly?
    static constexpr size_t STRIDE_SIZE = 8;
    /// @brief The derived stride mask from stride size.
    static constexpr uint64_t STRIDE_MASK = (1ULL << STRIDE_SIZE) - 1;
    /// @brief The lookup table itself.
    /// @warning These are constexpr, which explodes the compiler at a given limit.  Observed ~16... fyi.
    static constexpr auto STRIDE_TABLE = build_stride_table<Stride, STRIDE_SIZE>();



    /// @name Stride Application
    /// @{

    /**
    * @brief Applies the correct stride from the `STRIDE_TABLE` based on the mask of `value` and transforms it.
    * @tparam T Any supported integral (see concepts.hpp).
    * @param value The value to transform according to the `STRIDE_TABLE`.
    */
    template<AnySupportedIntegral T>
    static inline void apply_stride(T& value) {
        if constexpr(BuiltinIntegral<T>) {
            // Basic arithemtic and masking is fine on this path.
            const AffineStride::Stride& stride = AffineStride::STRIDE_TABLE[value & AffineStride::STRIDE_MASK];
            value = ((value * stride.multiply) + stride.add) >> stride.shift;
        } else {
            // GMP needs the UI extracted and cleaner calls to avoid temps.
            uint64_t u64_value = value.get_ui();
            const AffineStride::Stride& stride = AffineStride::STRIDE_TABLE[u64_value & AffineStride::STRIDE_MASK];
            mpz_mul_ui(value.get_mpz_t(), value.get_mpz_t(), stride.multiply);
            mpz_add_ui(value.get_mpz_t(), value.get_mpz_t(), stride.add);
            mpz_fdiv_q_2exp(value.get_mpz_t(), value.get_mpz_t(), stride.shift);
        }
    }



    /**
    * @brief Unroll 2 strides.
    * @note Neither this nor manual unrolling added performance when tested.
    * @tparam T Any supported integral (see concepts.hpp).
    * @param value The value to transform according to the `STRIDE_TABLE`.
    */
    template<AnySupportedIntegral T>
    static inline void apply_stride_2x(T& value) {
        apply_stride(value);
        apply_stride(value);
    }



    /**
    * @brief Unroll 4 strides.
    * @note Neither this nor manual unrolling added performance when tested.
    * @tparam T Any supported integral (see concepts.hpp).
    * @param value The value to transform according to the `STRIDE_TABLE`.
    */
    template<AnySupportedIntegral T>
    static inline void apply_stride_4x(T& value) {
        apply_stride_2x(value);
        apply_stride_2x(value);
    }



    /**
    * @brief Unroll 8 strides.
    * @note Neither this nor manual unrolling added performance when tested.
    * @tparam T Any supported integral (see concepts.hpp).
    * @param value The value to transform according to the `STRIDE_TABLE`.
    */
    template<AnySupportedIntegral T>
    static inline void apply_stride_8x(T& value) {
        apply_stride_4x(value);
        apply_stride_4x(value);
    }

    /// @}
}
