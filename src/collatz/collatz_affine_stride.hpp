#pragma once
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
*
* @note Theoretically, larger strides should be better.  Practically, the best performance is achieved when the table fits within
* the L1 cache.  Spilling into L2 destroys performance.  The defaults chosen are set in the CMakeLists.txt and can be overriden at
* compile time.
*/
namespace AffineStride {
    /// @brief Maximum stride length for the standard `Stride`.
    constexpr size_t MAX_STRIDE = 20;



    /// @brief A lightweight, data-only struct for the stride factors.
    struct Stride {
        uint32_t multiply = 1;       ///< Peak is all F's, which is ceil(log2(3^20)) == 32 bits.
        uint16_t add = 0;            ///< Peak is all F's, which is ceil(log2(~3324)) == 12 bits.
        uint8_t bits_required = 0;   ///< Number of bits required to apply this transformation (overflow risk).
        uint8_t shift = 0;           ///< Can hold 255 steps.  More than enough.
    };
    static_assert(sizeof(Stride) == 8);



    /**
    * @brief Builds a stride table at compile-time.
    * @tparam StrideType The type of stride struct to use.
    * @tparam bits The number of bits (steps) to build the table.  Final size is 2^bits.
    * @return An array of `Stride` or `LongStride` covering all starting bit patterns.
    */
    template<size_t bits>
    std::array<Stride, 1ULL << bits> build_stride_table() {
        static_assert(bits <= MAX_STRIDE, "Stride bits exceed MAX_STRIDE");

        // Build table.
        const uint64_t table_size = uint64_t(1) << bits;
        std::array<Stride, 1ULL << bits> stride_table{};

        // With the table preallocated, it can use index-based iteration.  Since it's looking at LSBs, we naturally get the order
        // and can simply loop through them sequentially.
        for (uint64_t i = 0; i < table_size; i++) {
            uint64_t n = i;
            Stride stride;
            for (size_t step_count = 0; step_count < bits; step_count++) {
                if (n & 1) {
                    stride.add = ((stride.add << 1) + stride.add) + (uint64_t(1) << stride.shift);
                    stride.multiply *= 3;
                    stride.shift++;
                    // Mimic the effects on n now.
                    n = (3 * n + 1) >> 1;
                } else {
                    stride.shift++;
                    // Mimic the effects on n now.
                    n >>= 1;
                }
            }
            // Calculate the number of bits required to apply this stride to a fixed-width type.  Add one for the .add factor.
            stride.bits_required = std::bit_width(stride.multiply - 1) + 1;
            // Assign to the table.
            stride_table[i] = stride;
        }

        return stride_table;
    }



    /**
    * @brief Build a stride table of only ones (..1111) at compile-time.
    * @tparam StrideType The type of stride struct to use.
    * @tparam bits The number of bits (steps) to build the table.  Since it's only ones, bits size == table size (+1).
    * @return An array of `Stride` or `LongStride` covering all consecutive ones in bits.
    */
    template<size_t bits>
    std::array<Stride, bits + 1> build_stride_table_of_ones() {
        static_assert(bits <= MAX_STRIDE, "Stride bits exceed MAX_STRIDE");

        // Build the table.
        std::array<Stride, bits + 1> stride_table_of_ones{};

        // Create the zero stride, which is no movement at all.
        Stride stride;
        stride_table_of_ones[0] = stride;

        // Now loop over consecutive ones, which is simply applying the F step to each previous stride.
        for (size_t i = 1; i <= bits; i++) {
            stride.multiply = stride.multiply * 3;
            stride.add = stride.add * 3 + (uint64_t(1) << stride.shift);
            stride.shift++;

            // Calculate the number of bits required to apply this stride to a fixed-width type.  Add one for the .add factor.
            stride.bits_required = std::bit_width(stride.multiply - 1) + 1;
            // Assign to the table.
            stride_table_of_ones[i] = stride;
        }

        return stride_table_of_ones;
    }



    /**
    * @struct Table
    * @brief A struct of constexpr stride data including a table, mask, and size based on `bits`.  Empirical testing shows that bit
    * sizes between 8-12 are most optimal, but the real threshold is L1 cache.  When it spills over into L2, performance drops.
    * @tparam bits The number of bits to build the table.  Final table size is 2^bits.  Blows up compilers ~16+.
    */
    template<size_t bits>
    struct Table {
        /// @brief The size of the stride, meaning how many accelerated F and G steps are taken.
        static constexpr size_t STRIDE_SIZE = bits;
        /// @brief The mask used to filter against any value in `apply_stride()`.
        static constexpr uint64_t MASK = (1ULL << STRIDE_SIZE) - 1;
        /// @brief The table of `Stride` objects, all computed at compile time.
        static const inline auto TABLE = build_stride_table<STRIDE_SIZE>();



        /**
        * @brief Gets the correct stride from `TABLE` for the given `value`.
        * @tparam T Any supported integral (see concepts.hpp).
        * @param value The value to fetch the stride for.
        */
        template<AnySupportedIntegral T>
        static inline const Stride& get_stride(const T& value) {
            if constexpr(FixedWidthIntegral<T>) {
                return TABLE[value & MASK];
            } else if constexpr(GMPIntegral<T>) {
                uint64_t u64_value = value.get_ui();
                return TABLE[u64_value & MASK];
            }
        }



        /**
        * @brief Applies a stride to `value`.  This version looks up the stride for you.  Next version requires you send it.
        * @tparam T Any supported integral (see concepts.hpp).
        * @param value The value to apply the stride to.
        */
        template<AnySupportedIntegral T>
        static inline void apply_stride(T& value) {
            if constexpr(FixedWidthIntegral<T>) {
                // Basic arithemtic and masking is fine on this path.
                const AffineStride::Stride& stride = get_stride(value);
                value = ((value * stride.multiply) + stride.add) >> stride.shift;
            } else if constexpr(GMPIntegral<T>) {
                // GMP needs the UI extracted and cleaner calls to avoid temps.
                const AffineStride::Stride& stride = get_stride(value);
                mpz_mul_ui(value.get_mpz_t(), value.get_mpz_t(), stride.multiply);
                mpz_add_ui(value.get_mpz_t(), value.get_mpz_t(), stride.add);
                mpz_fdiv_q_2exp(value.get_mpz_t(), value.get_mpz_t(), stride.shift);
            }
        }



        /**
        * @brief Applies a stride to `value`.  This version requires you send the stride.  Previous version does it for you.
        * @tparam T Any supported integral (see concepts.hpp).
        * @param value The value to apply the stride to.
        * @param stride The preselected stride to apply.
        */
        template<AnySupportedIntegral T>
        static inline void apply_stride(T& value, const Stride& stride) {
            if constexpr(FixedWidthIntegral<T>) {
                // Basic arithemtic is fine on this path.
                value = ((value * stride.multiply) + stride.add) >> stride.shift;
            } else if constexpr(GMPIntegral<T>) {
                // GMP needs the UI extracted and cleaner calls to avoid temps.
                mpz_mul_ui(value.get_mpz_t(), value.get_mpz_t(), stride.multiply);
                mpz_add_ui(value.get_mpz_t(), value.get_mpz_t(), stride.add);
                mpz_fdiv_q_2exp(value.get_mpz_t(), value.get_mpz_t(), stride.shift);
            }
        }
    };



    /// @brief Stride size when node building in Node::init() (and its helper) for fixed-width types.
    static constexpr size_t NODE_INIT_STRIDE_SIZE_FW = COLLATZ_NODE_INIT_STRIDE_SIZE_FW;
    /// @brief Stride size when node building in Node::init() (and its helper) for GMP types.
    static constexpr size_t NODE_INIT_STRIDE_SIZE_GMP = COLLATZ_NODE_INIT_STRIDE_SIZE_GMP;
    /// @brief Stride size when performing verification in Collatz::st_verify_X() method(s) for fixed-width types.
    static constexpr size_t VERIFY_STRIDE_SIZE_FW = COLLATZ_VERIFY_STRIDE_SIZE_FW;
    /// @brief Stride size when performing verification in Collatz::st_verify_X() method(s) for fixed-GMP types.
    static constexpr size_t VERIFY_STRIDE_SIZE_GMP = COLLATZ_VERIFY_STRIDE_SIZE_GMP;
    /// @brief Stride size when performing verification on a GPU.  Only applies to fixed-width types.
    static constexpr size_t VERIFY_STRIDE_SIZE_FW_CUDA = COLLATZ_VERIFY_STRIDE_SIZE_FW_CUDA;

    /// @brief The table for node init when fixed-width.
    using NodeInitFWTable = Table<NODE_INIT_STRIDE_SIZE_FW>;
    using NodeInitGMPTable = Table<NODE_INIT_STRIDE_SIZE_FW>;
    using VerifyFWTable = Table<VERIFY_STRIDE_SIZE_FW>;
    using VerifyGMPTable = Table<VERIFY_STRIDE_SIZE_GMP>;
    using VerifyFWCudaTable = Table<VERIFY_STRIDE_SIZE_FW_CUDA>;
}
