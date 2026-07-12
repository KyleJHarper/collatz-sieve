#pragma once
#include <mutex>
#include <cuda_runtime_api.h>
#include "collatz_affine_stride.hpp"
#include "collatz_affine_stride_cuda.cuh"



/// @namespace GPU
/// @brief Helper functions for GPU operations.  Largely Cuda, but whatever.
namespace GPU {
    /// @brief Simple flag to tell programs if a device is available.  True if found, false otherwise.
    #ifdef HAVE_CUDA
    static inline bool can_use_gpu() {
        int count = 0;
        if (cudaGetDeviceCount(&count) != cudaSuccess || count == 0) {
            return false;
        }
        return true;
    }
    #else
    static inline bool can_use_gpu() { return false; }
    #endif



    /// @brief Tracker for the affine table initialization.
    static std::once_flag initialize_stride_table_flag;

    /// @brief Initializes the affine stride table for the GPU.  Safe to repeatedly call.  Only one upload happens.
    static inline void initialize_stride_table() {
        // Build and upload the stride table to the GPU.
        static std::once_flag g_stride_table_uploaded;
        std::call_once(g_stride_table_uploaded, [] {
            cudaMemcpyToSymbol(
                AffineStrideCuda::device_stride_table<AffineStride::VERIFY_STRIDE_SIZE_FW_CUDA>
                , AffineStride::VerifyFWCudaTable::TABLE.data()
                , AffineStride::VerifyFWCudaTable::TABLE.size() * sizeof(AffineStride::Stride)
            );
        });
    }
}
