#include <cuda_runtime.h>
#include <cuda_runtime_api.h>
#include "verifier_gpu_result_data.hpp"
#include "concepts_for_cuda.hpp"
#include <cassert>
#include "collatz_affine_stride_cuda.cuh"
#include "collatz_constants_cuda.cuh"



#define CUDA_CHECK(call) do { \
    cudaError_t e = (call); \
    if (e != cudaSuccess) { \
        fprintf(stderr, "CUDA error %s:%d: %s\n", __FILE__, __LINE__, cudaGetErrorString(e)); \
        exit(1); \
    } \
} while(0)




/// @brief AI-slop for printing a 128-bit.
__device__ void print_uint128_decimal(unsigned __int128 val) {
    if (val == 0) { printf("0\n"); return; }

    char buffer[40]; // Max 39 digits + null terminator
    int i = 0;

    // Extract digits from right to left
    while (val > 0) {
        buffer[i++] = (char)('0' + (val % 10));
        val /= 10;
    }

    // Print the buffer in reverse order
    while (i > 0) {
        printf("%c", buffer[--i]);
    }
    printf("\n");
}



template <typename T>
__device__ inline int clz(T val) {
    static_assert(is_device_integral_v<T> , "clz only supports integral and 128-bit types.");

    if constexpr (sizeof(T) < 4) {
        // Less than 32 bits.  Need to promote and then subtract.
        return __clz((uint32_t)val) - (32 - (sizeof(T) * 8));
    } else if constexpr (sizeof(T) == 4) {
        // Exactly 32 bits.
        return __clz(val);
    }
    else if constexpr (sizeof(T) == 8) {
        // Exactly 64 bits.
        return __clzll(val);
    }
    else if constexpr (sizeof(T) == 16) {
        // Exactly 128 bits.
        uint64_t hi = (uint64_t)(val >> 64);
        uint64_t lo = (uint64_t)val;
        return (hi != 0) ? __clzll(hi) : (64 + __clzll(lo));
    }
}



template <typename T>
__device__ inline int ctz(T val) {
    static_assert(is_device_integral_v<T>, "ctz only supports integral and 128-bit types.");

    // Handle the zero edge-case to match `Bit`.
    if (val == 0) { return 0; }

    // FFS returns 1-based index, so subtract one.
    if constexpr (sizeof(T) <= 4) {
        // Less than or equal to 32 bits.
        return __ffs(val) - 1;
    }
    else if constexpr (sizeof(T) == 8) {
        // Exactly 64 bits.
        return __ffsll(val) - 1;
    }
    else if constexpr (sizeof(T) == 128 / 8) {
        // Exactly 128 bits.
        uint64_t lo = (uint64_t)val;
        uint64_t hi = (uint64_t)(val >> 64);
        return (lo != 0) ? (__ffsll(lo) - 1) : (64 + __ffsll(hi) - 1);
    }
}



/**
* @brief Verifies a value down to the sentinel value WITHOUT any checking or safety.  See `Collatz::st_verify_unsafe()`.
*
* Caller must:
*     * ensure `initial_value` is odd.
*     * ensure type `U` will not overflow.
*     * ensure `sentinel_value` is greater than 2, to avoid entering a 2-1-2 loop (mostly for GMP affine striding).
*
* @note In the event a number is divergent (cyclic), this method will hang.
* @param initial_value The starting value for a sequence.
* @param sentinel_value The value to stop at.  Must be greater than 2.
* @return True if the sequence reached the sentinel value, hangs otherwise.
*/
template<typename T>
__device__ inline bool verify_unsafe(const T& initial_value, const T& sentinel_value) {
    // Caller has checked for (or disregarded) overflow potential already.  Just blaze through the loop.  Make CPU happy.
    T current_value = initial_value;
    while (current_value >= sentinel_value) {
        // Must be odd here.
        current_value = (current_value << 1) + current_value + 1;
        // Must be even here.
        current_value >>= ctz(current_value);
    }

    return true;
}



/// @brief Verify any number down to 1 (or a custom value) as fast as possible.  See `Collatz::st_verify()` for details.
template<typename T, bool UseIVTable>
__device__ inline bool verify(const T& initial_value, const T& sentinel_value, bool& overflow) {
    // Start with no overflow.
    overflow = false;

    // If the value is even, it's automatically verified.
    if ((initial_value & 1) == 0) {
        return true;
    }

    // Make sure the sentinel value is at least 3, preventing infinite looping in edge cases with affine strides.
    T effective_sentinel_value = sentinel_value > T(2) ? sentinel_value : T(3);

    // If the table is viable, pick the known-safe type and pass the work to the "unsafe" verifier for speed.
    if constexpr (UseIVTable) {
        if (initial_value <= CollatzConstantsCuda::MAX_INITIAL_VALUE_BY_BIT.back()) {
            // Is promotion needed?  If not, send it as-is.
            if (initial_value <= CollatzConstantsCuda::get_max_initial_value_by_type<T>()) {
                return verify_unsafe(initial_value, effective_sentinel_value);
            }

            // Promotion needed.  Current type T cannot handle it.
            if constexpr(sizeof(T) * 8 < 64) {
                // Type T is less than 64 bits, just send it as a 64-bit.  Even uint32_t can't reach an overflow of 64-bit.
                // Max initial value of 64 bit is empirically shown to be 12,327,829,502 which is greater than 2^32 which is 4,294,967,296.
                uint64_t promoted_initial_value = initial_value;
                uint64_t promoted_effective_sentinel_value = effective_sentinel_value;
                return verify_unsafe(promoted_initial_value, promoted_effective_sentinel_value);
            } else if constexpr (sizeof(T) * 8 == 64) {
                // Dealing with T of exactly 64 bits.  Uint128_t can handle it.
                uint128_t promoted_initial_value = initial_value;
                uint128_t promoted_effective_sentinel_value = effective_sentinel_value;
                return verify_unsafe(promoted_initial_value, promoted_effective_sentinel_value);
            } else if constexpr (sizeof(T) * 8 == 128) {
                // Dealing with 128 bits.  Must flag as overflow.
                overflow = true;
                return false;
            }
        }
        //
        // Table above wasn't viable or used.  Process while tracking headroom bits for realtime safety.
    }

    // Pick a steady stride table.
    using StrideTable = AffineStrideCuda::VerifyFWCudaTable;

    // Use a stack variable for the temp.
    T current_value = initial_value;

    // Track headroom and make adjustments from Stride to prevent overflow efficiently.
    uint8_t headroom_bits = static_cast<uint8_t>(clz(current_value));

    // Run the loop.
    while (current_value >= effective_sentinel_value) {
        // Pick the next stride.
        const AffineStrideCuda::Stride& stride = StrideTable::get_stride(current_value);

        // Check headroom to see if we need promotion.
        if (headroom_bits < stride.bits_required) {
            if constexpr(sizeof(T) * 8 < 64) {
                // Type is less than 64 bits, just send it as a 64-bit.  Even uint32_t can't reach an overflow of 64-bit.
                // Max initial value of 64 bit is empirically shown to be 12,327,829,502 which is greater than 2^32 which is 4,294,967,296.
                uint64_t promoted_current_value = current_value;
                uint64_t promoted_effective_sentinel_value = effective_sentinel_value;
                return verify<uint64_t, UseIVTable>(promoted_current_value, promoted_effective_sentinel_value, overflow);
            } else if constexpr (sizeof(T) * 8 == 64) {
                // Dealing with T of exactly 64 bits.  Uint128_t is the next best candidate.
                uint128_t promoted_current_value = current_value;
                uint128_t promoted_effective_sentinel_value = effective_sentinel_value;
                return verify<uint128_t, UseIVTable>(promoted_current_value, promoted_effective_sentinel_value, overflow);
            } else if constexpr (sizeof(T) * 8 == 128) {
                // Dealing with 128 bits.  Can't go any further.  Flag overflow and return.
                overflow = true;
                return false;
            }
        }

        // No promotion needed.  Apply the stride.
        StrideTable::apply_stride(current_value, stride);

        // Adjust headroom according to Stride.
        headroom_bits += stride.shift;
        headroom_bits -= stride.bits_required;
    }

    // All done.  Return true.
    return true;
}



template<typename T, bool UseIVTable>
__global__ void gpu_verify_kernel(const T* values, GPUVerifierResultData* results, uint64_t count) {
    // Get our index and calculate a stride so we can loop here instead of relaunching.
    uint64_t base_index = blockIdx.x * blockDim.x + threadIdx.x;
    uint64_t stride = gridDim.x * blockDim.x;
    if (base_index >= count) { return; }

    for (uint64_t index = base_index; index < count; index += stride) {
        // Assume no overflow until detected.
        bool overflow = false;

        // Get the value from the buffer.
        T initial_value = *(values + index);

        // Verify it using a templated helper.
        bool verified = verify<T, UseIVTable>(initial_value, initial_value, overflow);

        if (overflow) {
            uint32_t overflow_index = atomicAdd(&(results->overflow_count), 1);
            if (overflow_index >= results->MAX_OVERFLOW) {
                results->overflow_exceeded = true;
            } else {
                results->overflow_indexes[overflow_index] = index;
            }
        }
        if (!overflow && !verified) {
            if constexpr(sizeof(T) * 8 == 64) {
                printf("Unable to verify value %lu\n", initial_value);
            } else if constexpr(sizeof(T) * 8 == 128) {
                printf("Unable to verify value ");
                print_uint128_decimal(initial_value);
            }
            assert(false);
        }
    }
}



template<typename T, bool UseIVTable>
void launch_gpu_verify_kernel(T* device_values, GPUVerifierResultData* device_results, uint64_t count, cudaStream_t stream) {
    // Define threads and blocks to send to the kernel.
    constexpr int threads_per_block = 128;
    int blocks = static_cast<int>((count + threads_per_block - 1) / threads_per_block);

    // Launch the kernel.
    gpu_verify_kernel<T, UseIVTable>
        <<<blocks, threads_per_block, 0, stream>>> (
        device_values
        , device_results
        , count
    );
}



// Explicit instantiations
/// @brief Explicit instantiation of 64-bit verifier without table lookups.
template void launch_gpu_verify_kernel<uint64_t, false>(uint64_t*, GPUVerifierResultData*, uint64_t, cudaStream_t);
/// @brief Explicit instantiation of 64-bit verifier with table lookups.
template void launch_gpu_verify_kernel<uint64_t, true>(uint64_t*, GPUVerifierResultData*, uint64_t, cudaStream_t);
/// @brief Explicit instantiation of 128-bit verifier without table lookups.
template void launch_gpu_verify_kernel<uint128_t, false>(uint128_t*, GPUVerifierResultData*, uint64_t, cudaStream_t);
/// @brief Explicit instantiation of 128-bit verifier with table lookups.
template void launch_gpu_verify_kernel<uint128_t, true>(uint128_t*, GPUVerifierResultData*, uint64_t, cudaStream_t);
