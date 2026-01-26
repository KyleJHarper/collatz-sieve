#include <cuda_runtime_api.h>
#include <stdint.h>
#include <stdio.h>
#include "concepts_for_cuda.hpp"
#include <cuda_runtime.h>
#include "step_counter_gpu_interface.hpp"
#include <thrust/device_ptr.h>
#include <thrust/device_vector.h>
#include <thrust/iterator/constant_iterator.h>
#include <thrust/sort.h>
#include <thrust/reduce.h>
#include <thrust/functional.h>


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
    tally_t steps = 0;
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
    if (base_index >= count) { return; }

    // Loop
    for(size_t index = base_index; index < count; index += stride) {
        T my_iv = *start_value + index;
        if (my_iv > max_value) { return; }
        d_steps[index] = collatz_step_count(my_iv);
        // If it's equal to 3XP1, we overflowed.
        if (d_steps[index] == get_max_3xp1<T>()) {
            atomicMin(overflow_index, static_cast<int>(index));
        }
    }
}




template<typename T>
class CollatzStepRunner {
    private:
    T* _unified_start_value_ptr = nullptr;
    size_t _buffer_size;


    public:
    CollatzStepRunner(
        size_t buffer_size
        , T* unified_start_value_ptr
    ) {
        // Save buffer and size.
        _buffer_size = buffer_size;
        _unified_start_value_ptr = unified_start_value_ptr;
    }

    ~CollatzStepRunner() {}

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

        // Everything can be done in this method.  Make device and host pools here.
        tally_t* host_steps = (tally_t*) std::malloc(_buffer_size * sizeof(tally_t));

        // Now the device memory.
        tally_t* d_steps = nullptr;
        CUDA_CHECK(cudaMalloc(&d_steps, _buffer_size * sizeof(tally_t)));

        // Now make a stream to sync with.
        cudaStream_t stream;
        CUDA_CHECK(cudaStreamCreate(&stream));

        // Create an overflow index for overflows.
        int* d_overflow_index = nullptr;
        CUDA_CHECK(cudaMallocManaged(&d_overflow_index, sizeof(int)));

        // Perform the main loop.
        while (*_unified_start_value_ptr <= max_value) {
            // Reset overflow.
            *d_overflow_index = INT_MAX;

            // Launch kernel
            collatz_steps_kernel<T><<<blocks, threads_per_block, 0, stream>>>(
                _unified_start_value_ptr
                , max_value
                , _buffer_size
                , d_steps
                , d_overflow_index
            );

            // Check launch for errors without global sync:
            CUDA_CHECK(cudaGetLastError());

            // Synchronzie the stream.
            CUDA_CHECK(cudaStreamSynchronize(stream));

            // Set a limit based on how many items we were able to scan.
            size_t limit = _buffer_size;
            T remaining_items = max_value - (*_unified_start_value_ptr) + 1;
            if (remaining_items < _buffer_size) {
                limit = remaining_items;
            }

            // Further restrict the limit to the overflow index, if one was set.
            limit = std::min(limit, static_cast<size_t>(*d_overflow_index));

            // Aggregate the values on-device with Thrust.
            thrust::device_ptr<tally_t> d_ptr(d_steps);
            thrust::sort(d_ptr, d_ptr + limit);
            thrust::device_vector<tally_t> unique_keys(limit);
            thrust::device_vector<tally_t> counts(limit);

            // Calculate a new end-pair from our reduction by the key (step count).
            auto new_end = thrust::reduce_by_key(
                d_ptr
                , d_ptr + limit
                , thrust::make_constant_iterator(1)
                , unique_keys.begin()
                , counts.begin()
            );

            // Get our counts, and copy the results into our keys (steps) and counts vectors.
            size_t num_unique = new_end.first - unique_keys.begin();
            std::vector<tally_t> h_keys(num_unique);
            std::vector<tally_t> h_counts(num_unique);
            thrust::copy(unique_keys.begin(), unique_keys.begin() + num_unique, h_keys.begin());
            thrust::copy(counts.begin(), counts.begin() + num_unique, h_counts.begin());

            // Merge aggregated counts into global_results
            for (size_t i = 0; i < num_unique; i++) {
                global_results->add_aggregate(level, h_keys[i], h_counts[i]);
            }

            // Bump the start value for the next loop or caller, and so our tracker can see it.
            *_unified_start_value_ptr += limit;

            // If we overflowed, we have to quit.
            if (*d_overflow_index < INT_MAX) {
                break;
            }
        }

        // Clean up.
        CUDA_CHECK(cudaFree(d_steps));
        std::free(host_steps);
        CUDA_CHECK(cudaFree(d_overflow_index));
        CUDA_CHECK(cudaStreamDestroy(stream));
    }
};

// Factory functions
template<typename T>
CollatzStepRunner<T>* create_runner(size_t buffer_size, T* unified_start_value_ptr) {
    return new CollatzStepRunner<T>(buffer_size, unified_start_value_ptr);
}

template<typename T>
void destroy_runner(CollatzStepRunner<T>* runner) {
    delete runner;
}

template<typename T>
void process_level_gpu(CollatzStepRunner<T>* runner, T max_value, size_t level, StepResults *global_results) {
    runner->compute_collatz_steps(max_value, level, global_results);
}

// Explicit instantiation definitions.
template class CollatzStepRunner<uint64_t>;
template class CollatzStepRunner<uint128_t>;
template CollatzStepRunner<uint64_t>* create_runner<uint64_t>(size_t, uint64_t*);
template CollatzStepRunner<uint128_t>* create_runner<uint128_t>(size_t, uint128_t*);
template void process_level_gpu<uint64_t>(CollatzStepRunner<uint64_t>*, uint64_t, size_t, StepResults*);
template void process_level_gpu<uint128_t>(CollatzStepRunner<uint128_t>*, uint128_t, size_t, StepResults*);
template void destroy_runner<uint64_t>(CollatzStepRunner<uint64_t>*);
template void destroy_runner<uint128_t>(CollatzStepRunner<uint128_t>*);
