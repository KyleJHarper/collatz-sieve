#pragma once

#include <cstdint>
#include "verifier_gpu_result_data.hpp"

#ifdef HAVE_CUDA
#include <cuda_runtime.h>
#endif


/// @brief Exposes the launch_gpu_verify_kernel in the .cu file.
template<typename T, bool UseIVTable>
void launch_gpu_verify_kernel(
    T* values
    , GPUVerifierResultData* results
    , uint64_t element_count
    , cudaStream_t stream
    , size_t base_multiplier = 1
    , size_t max_multiplier = 1
    , T scaling_factor = 0
);
