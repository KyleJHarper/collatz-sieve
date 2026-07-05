#pragma once

#include <cuda/std/array>
#include <cstddef>
#include <cstdint>
#include <cuda/std/bit>



/**
* @namespace AffineStrideCuda
* @brief A multi-step (stride) of accelerated Collatz functions `F` and `G` stored as an affine map.
*
* See "collatz_affine_stride.hpp" for more details.
*
* @note This is a stripped-down version of `AffineStride` for Cuda to use.
*/
namespace AffineStrideCuda {
    /// @brief Maximum stride length for the standard `Stride`.
    constexpr size_t MAX_STRIDE = 20;



    /// @brief Stride size when performing verification in Collatz::st_verify_X() method(s) for fixed-width types.
    static constexpr size_t VERIFY_STRIDE_SIZE_FW_CUDA = 12;



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
    __host__ __device__
    constexpr cuda::std::array<Stride, 1ULL << bits> build_stride_table() {
        static_assert(bits <= MAX_STRIDE, "Stride bits exceed MAX_STRIDE");

        // Build table.
        const uint64_t table_size = uint64_t(1) << bits;
        cuda::std::array<Stride, 1ULL << bits> stride_table{};

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
            stride.bits_required = cuda::std::bit_width(stride.multiply - 1) + 1;
            // Assign to the table.
            stride_table[i] = stride;
        }

        return stride_table;
    }



    /// @brief Build the compile-time, constant, device-only table of `Bits` size.
    template<size_t Bits>
    __device__ constexpr auto device_stride_table = build_stride_table<Bits>();



    /**
    * @struct Table
    * @brief A struct of constexpr stride data including a table, mask, and size based on `bits`.  Empirical testing shows that bit
    * sizes between 8-12 are most optimal, but the real threshold is L1 cache.  When it spills over into L2, performance drops.
    * @tparam bits The number of bits to build the table.  Final table size is 2^bits.  Blows up compilers ~16+.
    */
    template<size_t Bits>
    struct Table {
        static constexpr size_t STRIDE_SIZE = Bits;
        static constexpr uint64_t MASK = (1ULL << STRIDE_SIZE) - 1;

        /// @brief This will build a host-only table, which is probably never going to be used, but it costs virtually nothing.
        static constexpr auto TABLE = build_stride_table<STRIDE_SIZE>();

        /**
        * @brief Gets the correct stride from `TABLE` for the given `value`.
        * @tparam T Any supported integral (see concepts.hpp).
        * @param value The value to fetch the stride for.
        */
        template<typename T>
        __host__ __device__
        static inline const Stride& get_stride(T value) {
            #if defined(__CUDA_ARCH__)
                return device_stride_table<Bits>.data()[value & MASK];
            #else
                return TABLE.data()[value & MASK];
            #endif
        }


        /**
        * @brief Applies a stride to `value`.  This version looks up the stride for you.  Next version requires you send it.
        * @tparam T Any supported integral (see concepts.hpp).
        * @param value The value to apply the stride to.
        */
        template<typename T>
        __host__ __device__
        static inline void apply_stride(T& value) {
            const Stride& stride = get_stride(value);
            value = ((value * stride.multiply) + stride.add) >> stride.shift;
        }


        /**
        * @brief Applies a stride to `value`.  This version requires you send the stride.  Previous version does it for you.
        * @tparam T Any supported integral (see concepts.hpp).
        * @param value The value to apply the stride to.
        * @param stride The preselected stride to apply.
        */
        template<typename T>
        __host__ __device__
        static inline void apply_stride(T& value, const Stride& stride) {
            value = ((value * stride.multiply) + stride.add) >> stride.shift;
        }
    };


    using VerifyFWCudaTable = Table<VERIFY_STRIDE_SIZE_FW_CUDA>;
}
