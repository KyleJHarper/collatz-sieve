#pragma once
#include <unistd.h>


/**
* @struct VerifierExecutorPolicy
* @brief Flags and controls for the executors (CPU, GPU, etc) to dictate how they should run.
*/
struct VerifierExecutorPolicy {
    /// @brief Enables detailed metrics for step counts, striding, High-Water Mark stopping, etc.
    bool detailed_metrics = false;



    /// @brief Enables the lookup of max initial values inside `CollatzConstants` to prevent type overflows instead of tracking
    /// headroom bits.
    /// @note The verifier will switch to headroom bits when the table is exhausted.
    bool enable_max_iv_table = true;



    /// @brief Controls the number of bytes to use on the GPU.  When 0, `GPUVerifier` targets 80%.
    size_t gpu_buffer_limit = 0;



    /// @brief Determines how many scaling factors will apply on each GPU run.  See `GPUVerifier` for details.
    size_t scales_per_run = 10;
};
