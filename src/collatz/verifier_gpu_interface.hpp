#pragma once

#include <cstdint>
#include "verifier_gpu_result_data.hpp"

#ifdef HAVE_CUDA
#include <cuda_runtime.h>
#endif


/// @brief Expose the launch_gpu_verify_kernel in the .cu file.
template<typename T, bool UseIVTable>
void launch_gpu_verify_kernel(
    T* device_values,
    GPUVerifierResultData* device_results,
    uint64_t count,
    cudaStream_t stream
);
