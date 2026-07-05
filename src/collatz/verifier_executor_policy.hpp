#pragma once



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
};
