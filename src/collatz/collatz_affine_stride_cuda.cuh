#pragma once

#include <cuda/std/array>
#include <cstddef>
#include <cstdint>
#include <cuda/std/bit>
#include <stdexcept>



/**
* @namespace AffineStrideCuda
* @brief A multi-step (stride) of accelerated Collatz functions `F` and `G` stored as an affine map.
*
* See "collatz_affine_stride.hpp" for more details.
*
* @note This is a stripped-down version of `AffineStride` for Cuda to use.
*/
namespace AffineStrideCuda {
    /// @brief A lightweight, data-only struct for the stride factors.
    struct Stride {
        uint32_t multiply = 1;       ///< Peak is all F's, which is ceil(log2(3^20)) == 32 bits.
        uint16_t add = 0;            ///< Peak is all F's, which is ceil(log2(~3324)) == 12 bits.
        uint8_t bits_required = 0;   ///< Number of bits required to apply this transformation (overflow risk).
        uint8_t shift = 0;           ///< Can hold 255 steps.  More than enough.
    };
    static_assert(sizeof(Stride) == 8);



    /// @brief Build the compile-time, constant, device-only table of `Bits` size.
    template<size_t Bits>
    __device__ Stride device_stride_table[1ULL << Bits];



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



        /**
        * @brief Gets the correct stride from `TABLE` for the given `value`.
        * @tparam T Any supported integral (see concepts.hpp).
        * @param value The value to fetch the stride for.
        */
        template<typename T>
        __device__
        static inline const Stride& get_stride(T value) {
            return device_stride_table<Bits>[value & MASK];
        }



        /**
        * @brief Applies a stride to `value`.  Requires you send the stride.  Previous version does it for you.
        * @tparam T Any supported integral (see concepts.hpp).
        * @param value The value to apply the stride to.
        * @param stride The preselected stride to apply.
        */
        template<typename T>
        __device__
        static inline void apply_stride(T& value, const Stride& stride) {
            value = ((value * stride.multiply) + stride.add) >> stride.shift;
        }
    };



    /// @brief Uploads the host-computed table into device memory. Call exactly once, before any
    /// kernel that uses get_stride() launches.
    template<size_t Bits>
    void upload_stride_table_to_device() {
        constexpr auto& host_table = Table<Bits>::TABLE;
        cudaError_t status = cudaMemcpyToSymbol(
            device_stride_table<Bits>
            , host_table.data()
            , sizeof(Stride) * (1ULL << Bits)
            , 0
            , cudaMemcpyHostToDevice
        );
        if (status != cudaSuccess) {
            throw std::runtime_error(std::string("Failed to upload stride table: ") + cudaGetErrorString(status));
        }
    }

    using VerifyFWCudaTable = Table<COLLATZ_VERIFY_STRIDE_SIZE_FW_CUDA>;
}
