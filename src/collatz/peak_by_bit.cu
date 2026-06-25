#include <cuda_runtime_api.h>
#include <stdint.h>
#include <stdio.h>
#include "concepts_for_cuda.hpp"
#include <cuda_runtime.h>


#define CUDA_CHECK(call) do { \
    cudaError_t e = (call); \
    if (e != cudaSuccess) { \
        fprintf(stderr, "CUDA error %s:%d: %s\n", __FILE__, __LINE__, cudaGetErrorString(e)); \
        exit(1); \
    } \
} while(0)




/**
* @brief Stores the highest integer which can take 3x + 1 without overflowing a given bit width.
* @note This only supports 8, 16, 32, 64, and 128 bit widths.
* @note This only supports unsigned types.
* @note Copied from `CollatzConstants`
*/
__host__ __device__ constexpr uint128_t MAX_3XP1[5] = {
    ((uint128_t(1) <<  8) - 1 - 1) / 3,   // 8 bit
    ((uint128_t(1) << 16) - 1 - 1) / 3,   // 16 bit
    ((uint128_t(1) << 32) - 1 - 1) / 3,   // 32 bit
    ((uint128_t(1) << 64) - 1 - 1) / 3,   // 64 bit
    ((uint128_t(1) << 127) + ((uint128_t(1) << 127) - 1) - 1) / 3  // 128 bit
    // Requires a little juggling to avoid overflow.  Yay PEMDAS!
};



/**
* @brief Helper to return the correct `MAX_3XP1` for the type `T`.
* @tparam T Any supported integral (see concepts.hpp).
*/
template<typename T>
__host__ __device__ inline constexpr uint128_t get_max_3xp1() {
    static_assert(is_device_integral_v<T>, "T must be uint32_t, uint64_t, or uint128_t");
    switch (sizeof(T) * 8) {
        case   8: return MAX_3XP1[0];
        case  16: return MAX_3XP1[1];
        case  32: return MAX_3XP1[2];
        case  64: return MAX_3XP1[3];
        case 128: return MAX_3XP1[4];
    }
}



/**
* @brief Find the peak value of an initial value, optinally stopping if we reach below HWM.
* @param initial_value The starting value to begin a Collatz sequence.
* @tparam T Any device-supported integral, which is 32-, 64-, and 128-bit for modern CUDA.
* @return The peak value found.  If the next 3x+1 would overflow, we set peak to get_max_3xp1<T>().  Caller must check.
*/
template<typename T>
__device__ T collatz_get_peak(
    T initial_value
) {
    static_assert(is_device_integral_v<T>, "T must be uint32_t, uint64_t, or uint128_t");
    T current_step = initial_value;
    T peak = initial_value;

    while (current_step > 1) {
        // Store the peak if it's higher.
        if (current_step > peak) {
            peak = current_step;
        }

        // We always skip after HWM, leave when we're below it.
        if (current_step < initial_value) {
            break;
        }

        // Perform the step.
        if ((current_step & 1) == 1) {
            // It's odd.  Grow without overflowing.
            if (current_step > get_max_3xp1<T>()) {
                peak = get_max_3xp1<T>();
                break;
            }
            // 3x+1
            current_step = (current_step << 1) + current_step + 1;
            // Check for peak again.  We can't hit HWM with a 3x+1 step.
            if (current_step > peak) {
                peak = current_step;
            }
        }
        // We have an even at this point no matter what.  Shift it.
        current_step >>= (__ffs(current_step) - 1);
    }

    return peak;
}



/**
* @brief Kernel to capture peaks of Collatz sequences.
* @param base_initial_value The first initial value to find peak of.
* @param max_allowed_value The highest value a sequence can reach before triggering overflow.
* @param count The number of loops to run in this kernel.
* @param d_peaks Device memory to write peaks to as they're found.
* @param failing_index The index this method returns if a sequence exceeds `max_allowed_value`.
* @param overflow_index The index this method returns if a sequence overflows the data type.
* @tparam T Any device-supported integral, which is 32-, 64-, and 128-bit for modern CUDA.
*/
template<typename T>
__global__ void collatz_peaks_kernel(
    T base_initial_value
    , T max_allowed_value
    , size_t count
    , T* d_peaks
    , int* failing_index
    , int* overflow_index
) {
    // Get our index.
    size_t base_index = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride = gridDim.x * blockDim.x;
    if (base_index >= count) { return; }

    T my_peak;
    for (size_t index = base_index; index < count; index += stride) {
        // Build our IV and then run it.
        T my_iv = base_initial_value + index;

        // Store the peak no matter what.
        my_peak = collatz_get_peak(my_iv);

        // If it's equal to 3XP1, we overflowed.
        if (my_peak == get_max_3xp1<T>()) {
            atomicMin(overflow_index, static_cast<int>(index));
            d_peaks[index] = my_peak;
        }

        // If it exceeded, flag that.
        if (my_peak > max_allowed_value) {
            atomicMin(failing_index, static_cast<int>(index));
            d_peaks[index] = my_peak;
        }
    }
}



/**
* @class CollatzPeakRunner
* @brief Unifies a lot of the peak-by-bit logic into a single runner class for homogenous interaction with CPU.
* @tparam T Any device-supported integral, which is 32-, 64-, and 128-bit for modern CUDA.
*/
template<typename T>
class CollatzPeakRunner {
    private:
    /// @brief Device memory to write peaks to as they're found.
    T* _d_peaks = nullptr;
    /// @brief Pointer to pinned memory for GPU<->CPU synchronization.
    T* _h_peaks_pin = nullptr;
    /// @brief Pointer to a unified memory location for base_initial_value.
    T* _unified_base_initial_value_ptr = nullptr;
    /// @brief Pointer to a unified memory location for overflow index when an overflow happens.
    int* _unified_overflow_index_ptr = nullptr;
    /// @brief Pointer to a unified memory location for failing index when a sequence exceeds max value.
    int* _unified_failing_index_ptr = nullptr;
    /// @brief A memory stream for sychronizing host and device data.
    cudaStream_t _stream;
    /// @brief Buffer size for controlling work chunking sent to kernels.
    size_t _buffer_size;



    public:
    /// @name Lifecycle Management
    /// @{

    /**
    * @brief Constructor to establish host and device memory in preparation for work.
    * @param host_buffer Pointer to host memory to synchronize back to after computing work on the device.
    * @param buffer_size Nuber of elements to process in a single pass, spread across grids/dimensions.
    * @param unified_base_initial_value_ptr Pointer to a unified host-device memory location for base initial value.
    * @param unified_overflow_index Pointer to a unified memory location for overflow index when an overflow happens.
    * @param unified_failing_index Pointer to a unified memory location for failing index when a sequence exceeds max value.
    */
    CollatzPeakRunner(
        T* host_buffer
        , size_t buffer_size
        , T* unified_base_initial_value_ptr
        , int* unified_overflow_index
        , int* unified_failing_index
    ) {
        // Save buffer and size.
        _h_peaks_pin = host_buffer;
        _buffer_size = buffer_size;
        _unified_base_initial_value_ptr = unified_base_initial_value_ptr;
        _unified_overflow_index_ptr = unified_overflow_index;
        _unified_failing_index_ptr = unified_failing_index;

        // Allocate device buffer
        CUDA_CHECK(cudaMalloc(&_d_peaks, buffer_size * sizeof(T)));

        // Register host memory as pinned
        CUDA_CHECK(cudaHostRegister(_h_peaks_pin, buffer_size * sizeof(T), cudaHostRegisterDefault));

        // Create stream
        CUDA_CHECK(cudaStreamCreate(&_stream));
    }



    /// @brief Destructor frees device memory, unregisters the host peaks pin, and destroys the sync stream.
    ~CollatzPeakRunner() {
        cudaFree(_d_peaks);
        cudaHostUnregister(_h_peaks_pin);
        cudaStreamDestroy(_stream);
    }

    /// @}



    /// @brief Host code to compute a bunch of peaks and then bring them to the host.
    /// @param bit The current bit being searched for.  I.e.: max value will be `2^bit` - 1.
    void compute_collatz_peak(size_t bit) {
        const int threads_per_block = 128;
        int blocks = (_buffer_size + threads_per_block - 1) / threads_per_block;
        T max_allowed_value = (T(1) << bit) - 1;
        *_unified_failing_index_ptr = INT_MAX;
        *_unified_overflow_index_ptr = INT_MAX;

        while (true) {
            // Launch kernel
            collatz_peaks_kernel<T><<<blocks, threads_per_block, 0, _stream>>>(
                *_unified_base_initial_value_ptr
                , max_allowed_value
                , _buffer_size
                , _d_peaks
                , _unified_failing_index_ptr
                , _unified_overflow_index_ptr
            );

            // Synchronize to get updated values.
            cudaStreamSynchronize(_stream);

            // If we hit and overflow or found a failing index, break out.
            // If we overflowed, sad panda :(
            // Overflow means the work we did is worthless, so we don't need to copy.  Just quit.
            if (*_unified_overflow_index_ptr < INT_MAX || *_unified_failing_index_ptr < INT_MAX) {
                break;
            }

            // Didn't find it ... so update the base IV and try a new batch.
            *_unified_base_initial_value_ptr += _buffer_size;
        }

        // We overflowed or found a peak.  We can copy this result into the host buffer and let the CPU logic find/handle the failing index.
        CUDA_CHECK(cudaMemcpyAsync(_h_peaks_pin, _d_peaks, _buffer_size * sizeof(T), cudaMemcpyDeviceToHost, _stream));
        CUDA_CHECK(cudaStreamSynchronize(_stream));
    }
};



/// @brief Creates a `CollatzPeakRunner`; exposed for use with host code via an interface.
template<typename T>
CollatzPeakRunner<T>* create_runner(T* host_buffer, size_t buffer_size, T* unified_base_initial_value_ptr, int* unified_overflow_index, int* unified_failing_index) {
    return new CollatzPeakRunner<T>(host_buffer, buffer_size, unified_base_initial_value_ptr, unified_overflow_index, unified_failing_index);
}



/// @brief Destroys a `CollatzPeakRunner`; exposed for use with host code via an interface.
template<typename T>
void destroy_runner(CollatzPeakRunner<T>* runner) {
    delete runner;
}



/// @brief Executes the `compute_collatz_peak()` on the GPU; exposed for use with the host code via an interface.
template<typename T>
void find_max_iv_for_bit_gpu(CollatzPeakRunner<T>* runner, size_t bit) {
    runner->compute_collatz_peak(bit);
}



// Explicit instantiations
/// @brief Explicit declaration of 64-bit runner.
template class CollatzPeakRunner<uint64_t>;
/// @brief Explicit declaration of 128-bit runner.
template class CollatzPeakRunner<__uint128_t>;
/// @brief Explicit declaration of 64-bit create_runner method.
template CollatzPeakRunner<uint64_t>* create_runner(uint64_t*, size_t, uint64_t*, int*, int*);
/// @brief Explicit declaration of 128-bit create_runner method.
template CollatzPeakRunner<__uint128_t>* create_runner(__uint128_t*, size_t, uint128_t*, int*, int*);
/// @brief Explicit declaration of 64-bit destroy_runner method.
template void destroy_runner(CollatzPeakRunner<uint64_t>*);
/// @brief Explicit declaration of 128-bit destroy_runner method.
template void destroy_runner(CollatzPeakRunner<__uint128_t>*);
/// @brief Explicit declaration of 64-bit find_max_iv_for_bit_gpu method.
template void find_max_iv_for_bit_gpu(CollatzPeakRunner<uint64_t>*, size_t);
/// @brief Explicit declaration of 128-bit find_max_iv_for_bit_gpu method.
template void find_max_iv_for_bit_gpu(CollatzPeakRunner<__uint128_t>*, size_t);
