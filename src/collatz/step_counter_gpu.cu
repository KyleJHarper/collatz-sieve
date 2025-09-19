#include <cuda_runtime_api.h>
#include <stdint.h>
#include "concepts_for_cuda.hpp"
#include <cuda_runtime.h>
#include "step_counter_gpu_interface.hpp"


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
// Collatz Get Step Count
// Find the number of steps for a given IV
//
// If the next 3x+1 would overflow, we set step_count to get_max_3xp1<T>().  Caller must check.
//
template<typename T>
__device__ tally_t collatz_step_count(T initial_value) {
    T steps = 0;
    T current_step = initial_value;
    while (current_step > 1) {
        steps += 1;
        if (current_step % 2 == 0) {
            current_step /= 2;
        } else {
            if (current_step > get_max_3xp1<T>()) {
                steps = get_max_3xp1<T>();
                break;
            }
            current_step *= 3;
            current_step += 1;
        }
    }

    return steps;
}



//
// Kernel to capture the steps of Collatz sequences.
//
template<typename T>
__global__ void collatz_steps_kernel(
    T* start_value
    , T max_value
    , size_t count
    , tally_t* d_steps
    , int* overflow_index
) {
    // Compute our index and stride, so we can roll more work into a single resultset.
    size_t base_index = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride = gridDim.x * blockDim.x;
    T steps = 0;
    if (base_index >= count) { return; }

    // Loop
    for(size_t index = base_index; index < count; index += stride) {
        T my_iv = *start_value + index;
        if (my_iv > max_value) { return; }
        d_steps[index] = collatz_step_count(my_iv);
        // If it's equal to 3XP1, we overflowed.
        if (d_peaks[index] == get_max_3xp1<T>()) {
            atomicMin(overflow_index, static_cast<int>(index));
        }
    }
}




template<typename T>
class CollatzStepRunner {
    private:
    T* _d_steps = nullptr;
    T* _h_steps_pin = nullptr;
    T* _unified_start_value_ptr = nullptr;
    cudaStream_t _stream;
    size_t _buffer_size;


    public:
    CollatzStepRunner(
        T* host_buffer
        , size_t buffer_size
        , T* unified_start_value_ptr
    ) {
        // Save buffer and size.
        _h_steps_pin = host_buffer;
        _buffer_size = buffer_size;
        _unified_start_value_pointer = unified_start_value_ptr;

        // Allocate device buffer
        CUDA_CHECK(cudaMalloc(&_d_steps, buffer_size * sizeof(T)));

        // Register host memory as pinned
        CUDA_CHECK(cudaHostRegister(_h_steps_pin, buffer_size * sizeof(T), cudaHostRegisterDefault));

        // Create stream
        CUDA_CHECK(cudaStreamCreate(&_stream));
    }

    ~CollatzStepRunner() {
        cudaFree(_d_steps);
        cudaHostUnregister(_h_steps_pin);
        cudaStreamDestroy(_stream);
    }

    //
    // Host code to compute a bunch of peaks and then bring them to the host.  We will alloc on the device for you.
    //
    void compute_collatz_steps(
        T max_value
        , size_t level
        , StepResults* global_results
    ) {
        const int threads_per_block = 128;
        int blocks = (_buffer_size + threads_per_block - 1) / threads_per_block;
        int overflow_index = INT_MAX;

        while (*start_value <= max_value) {
            // Launch kernel
            collatz_steps_kernel<T><<<blocks, threads_per_block, 0, _stream>>>(
                *_unified_start_value
                , max_allowed_value
                , _buffer_size
                , _d_steps
                , &overflow_index
            );

            // Synchronize to get updated values.
            cudaDeviceSynchronize();
            CUDA_CHECK(cudaMemcpyAsync(_h_steps_pin, _d_steps, _buffer_size * sizeof(T), cudaMemcpyDeviceToHost, _stream));
            CUDA_CHECK(cudaStreamSynchronize(_stream));

            // Set a limit based on whether we overflowed.
            size_t limit = std::min(_buffer_size, overflow_index);

            // Bump the start value for the next loop or caller, and so our tracker can see it.
            *start_value += limit;

            // We now have a host-side array of type T, add the results to the global tracker.
            for(size_t index = 0; index < limit; index++) {
                global_results->add(level, _h_steps_pin[index]);
            }

            // If we overflowed, we have to quit.
            if (overflow_index < INT_MAX) {
                break;
            }

        }
    }
};

// Factory functions
template<typename T>
CollatzStepRunner<T>* create_runner(T* host_buffer, size_t buffer_size, T* unified_start_value) {
    return new CollatzStepRunner<T>(host_buffer, buffer_size, unified_start_value);
}

template<typename T>
void destroy_runner(CollatzStepRunner<T>* runner) {
    delete runner;
}

template<typename T>
void process_level_gpu(CollatzStepRunner<T>* runner, T max_value, size_t level, StepResults *global_results) {
    runner->compute_collatz_steps(max_value, level, global_results);
}

// // Explicit instantiations
// template class CollatzPeakRunner<uint64_t>;
// template class CollatzPeakRunner<__uint128_t>;
// template CollatzPeakRunner<uint64_t>* create_runner(uint64_t*, size_t, uint64_t*, int*, int*);
// template CollatzPeakRunner<__uint128_t>* create_runner(__uint128_t*, size_t, uint128_t*, int*, int*);
// template void destroy_runner(CollatzPeakRunner<uint64_t>*);
// template void destroy_runner(CollatzPeakRunner<__uint128_t>*);
// template void find_max_iv_for_bit_gpu(CollatzPeakRunner<uint64_t>*, size_t, bool);
// template void find_max_iv_for_bit_gpu(CollatzPeakRunner<__uint128_t>*, size_t, bool);
