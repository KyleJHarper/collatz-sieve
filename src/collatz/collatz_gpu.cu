#include <cuda_runtime_api.h>
#include <stdint.h>
#include <stdio.h>
#include "concepts_for_cuda.hpp"
#include <cuda_runtime.h>
#include "collatz_gpu_interface.hpp"

#define CUDA_CHECK(call) do { \
  cudaError_t e = (call); \
  if (e != cudaSuccess) { \
    fprintf(stderr, "CUDA error %s:%d: %s\n", __FILE__, __LINE__, cudaGetErrorString(e)); \
    exit(1); \
  } \
} while(0)


//
// Stolen from CollatzConstants
//
// Trying to perform 3X+1 on any value higher than this would overflow a 64-bit unsigned integer.
__host__ __device__ constexpr uint128_t MAX_3XP1[5] = {
    ((uint128_t(1) <<  8) - 1 - 1) / 3,   // 8 bit
    ((uint128_t(1) << 16) - 1 - 1) / 3,   // 16 bit
    ((uint128_t(1) << 32) - 1 - 1) / 3,   // 32 bit
    ((uint128_t(1) << 64) - 1 - 1) / 3,   // 64 bit
    ((uint128_t(1) << 127) + ((uint128_t(1) << 127) - 1) - 1) / 3  // 128 bit
    // Requires a little juggling to avoid overflow.  Yay PEMDAS!
};
//
// Now a helper for it.
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



//
// Collatz Get Peak
// Find the peak value of an initial value, optinally stopping if we reach below HWM.
//
// If the next 3x+1 would overflow, we set peak to get_max_3xp1<T>().  Caller must check.
//
template<typename T>
__device__ T collatz_get_peak(
    T initial_value
    , bool skip_after_hwm
) {
    static_assert(is_device_integral_v<T>, "T must be uint32_t, uint64_t, or uint128_t");
    T current_step = initial_value;
    T peak = initial_value;

    while (current_step > 1) {
        // Store the peak if it's higher.
        if (current_step > peak) {
            peak = current_step;
        }

        // If we're skipping after HWM, leave when we're below it.
        if (skip_after_hwm && current_step < initial_value) {
            break;
        }

        // Perform the step.
        if (current_step % 2 == 0) {
            current_step /= 2;
        } else {
            if (current_step > get_max_3xp1<T>()) {
                peak = get_max_3xp1<T>();
                break;
            }
            current_step *= 3;
            current_step += 1;
        }
    }

    return peak;
}



//
// Kernel to capture peaks of Collatz sequences.
//
template<typename T>
__global__ void collatz_peaks_kernel(
    T* base_initial_value
    , T max_allowed_value
    , size_t count
    , T* d_peaks
    , int* failing_index
    , int* overflow_index
    , bool skip_after_hwm
) {
    // Get our index.
    size_t index = blockIdx.x * blockDim.x + threadIdx.x;
    if (index >= count) { return; }

    // Build our IV and then run it.
    T my_iv = *base_initial_value + index;

    // Store the peak no matter what.
    d_peaks[index] = collatz_get_peak(my_iv, skip_after_hwm);

    // If it's equal to 3XP1, we overflowed.
    if (d_peaks[index] == get_max_3xp1<T>()) {
        atomicMin(overflow_index, static_cast<int>(index));
    }

    // If it exceeded, flag that.
    if (d_peaks[index] > max_allowed_value) {
        atomicMin(failing_index, static_cast<int>(index));
    }
}



template<typename T>
class CollatzPeakRunner {
    private:
    T* _d_peaks = nullptr;
    T* _h_peaks_pin = nullptr;
    T* _unified_base_initial_value_ptr = nullptr;
    int* _unified_overflow_index_ptr = nullptr;
    int* _unified_failing_index_ptr = nullptr;
    cudaStream_t _stream;
    size_t _buffer_size;


    public:
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

    ~CollatzPeakRunner() {
        cudaFree(_d_peaks);
        cudaHostUnregister(_h_peaks_pin);
        cudaStreamDestroy(_stream);
    }

    //
    // Host code to compute a bunch of peaks and then bring them to the host.  We will alloc on the device for you.
    //
    void compute_collatz_peak(
        size_t bit
        , bool skip_after_hwm
    ) {
        const int threads_per_block = 512;
        int blocks = (_buffer_size + threads_per_block - 1) / threads_per_block;
        T max_allowed_value = (T(1) << bit) - 1;
        *_unified_failing_index_ptr = INT_MAX;
        *_unified_overflow_index_ptr = INT_MAX;

        while (true) {
            // Launch kernel
            collatz_peaks_kernel<T><<<blocks, threads_per_block, 0, _stream>>>(
                _unified_base_initial_value_ptr
                , max_allowed_value
                , _buffer_size
                , _d_peaks
                , _unified_failing_index_ptr
                , _unified_overflow_index_ptr
                , skip_after_hwm
            );

            // Synchronize to get updated values.
            cudaDeviceSynchronize();

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

// Factory functions
template<typename T>
CollatzPeakRunner<T>* create_runner(T* host_buffer, size_t buffer_size, T* unified_base_initial_value_ptr, int* unified_overflow_index, int* unified_failing_index) {
    return new CollatzPeakRunner<T>(host_buffer, buffer_size, unified_base_initial_value_ptr, unified_overflow_index, unified_failing_index);
}

template<typename T>
void destroy_runner(CollatzPeakRunner<T>* runner) {
    delete runner;
}

template<typename T>
void compute_peak(CollatzPeakRunner<T>* runner, size_t bit, bool skip_after_hwm) {
    runner->compute_collatz_peak(bit, skip_after_hwm);
}

// Explicit instantiations
template class CollatzPeakRunner<uint64_t>;
template class CollatzPeakRunner<__uint128_t>;
template CollatzPeakRunner<uint64_t>* create_runner(uint64_t*, size_t, uint64_t*, int*, int*);
template CollatzPeakRunner<__uint128_t>* create_runner(__uint128_t*, size_t, uint128_t*, int*, int*);
template void destroy_runner(CollatzPeakRunner<uint64_t>*);
template void destroy_runner(CollatzPeakRunner<__uint128_t>*);
template void compute_peak(CollatzPeakRunner<uint64_t>*, size_t, bool);
template void compute_peak(CollatzPeakRunner<__uint128_t>*, size_t, bool);




//
// Testing of a single IV for now.  Should probably make this a test suite.
//
template<typename T>
__global__ void test_single_iv_kernel(
    const T* iv_ptr,
    bool skip_after_hwm,
    T* out_peak,
    int* out_overflow_flag,
    T* out_max3xp1 // returns the get_max_3xp1<uint64_t>() value for sanity
) {
    T iv = *iv_ptr;
    T peak = collatz_get_peak<T>(iv, skip_after_hwm);
    *out_peak = peak;
    *out_overflow_flag = (peak == (T)get_max_3xp1<T>()) ? 1 : 0;
    *out_max3xp1 = (T)get_max_3xp1<T>();
}

template<typename T>
void run_single_iv(
    T iv,
    bool skip_after_hwm,
    T* h_out_peak,
    int* h_out_overflow_flag,
    T* h_out_max3xp1 // returns the get_max_3xp1<uint64_t>() value for sanity
) {
    // Device pointers
    T *d_iv = nullptr;
    T *d_peak = nullptr, *d_max3xp1 = nullptr;
    int *d_overflow_flag = nullptr;

    cudaMalloc(&d_iv, sizeof(T));
    cudaMalloc(&d_peak, sizeof(T));
    cudaMalloc(&d_max3xp1, sizeof(T));
    cudaMalloc(&d_overflow_flag, sizeof(int));
    cudaMemcpy(d_iv, &iv, sizeof(T), cudaMemcpyHostToDevice);

    // Launch with a single thread (<<<1,1>>>)
    test_single_iv_kernel<T><<<1,1>>>(
        d_iv,
        skip_after_hwm,
        d_peak,
        d_overflow_flag,
        d_max3xp1
    );

    // Synchronize and save the values.
    cudaDeviceSynchronize();
    cudaMemcpy(h_out_peak, d_peak, sizeof(T), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_out_overflow_flag, d_overflow_flag, sizeof(int), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_out_max3xp1, d_max3xp1, sizeof(T), cudaMemcpyDeviceToHost);

    // Free
    cudaFree(d_iv);
    cudaFree(d_peak);
    cudaFree(d_max3xp1);
    cudaFree(d_overflow_flag);
}
// explicit instantiations for run_single_iv
template void run_single_iv<uint64_t>(uint64_t, bool, uint64_t*, int*, uint64_t*);

// if you plan to use 128-bit version too (when device supports it)
template void run_single_iv<__uint128_t>(__uint128_t, bool, __uint128_t*, int*, __uint128_t*);
