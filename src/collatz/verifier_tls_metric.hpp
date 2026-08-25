#pragma once
#include <stdint.h>
#include "verifier_metric.hpp"



/**
* @struct VerifierTLSMetrics
* @brief Holds fast, non-atomic data for threads to leverage for tracking.  Used by `VerifierMetrics` to get updates.
*/
struct VerifierTLSMetric {
    /// @brief True nodes (values) verified.
    uint64_t nodes_verified = 0;



    /// @brief Total number of steps taken
    /// @note Only available when `DetailedMetrics` are enabled.
    uint64_t steps_total = 0;



    /// @brief Number of steps skipped (or would've been skipped) by hitting Abort at Stopping Time.
    /// @note Only available when `DetailedMetrics` are enabled.
    uint64_t steps_skippable_by_ast = 0;



    /// @brief Number of steps skipped (or would've been skipped) by using Affine Strides alone.
    /// @note Only available when `DetailedMetrics` are enabled.
    uint64_t steps_skippable_by_affine_stride = 0;



    /// @brief Number of steps skipped (or would've been skipped) by using Affine Strides before hitting AST.
    /// @note Only available when `DetailedMetrics` are enabled.
    uint64_t steps_skippable_by_affine_stride_before_ast = 0;



    /**
    * @brief Write the TLS accumulated values to a `VerifierMetric` object.  Then zero out this object's counters.
    * @param metrics An instance of `VerifierMetric` whose atomic counters will be updated via `fetch_add()`.
    * @param do_reset Controls whether counters are reset after flushing.  Default true.
    */
    void flush(VerifierMetric& metrics, bool do_reset) {
        // Atomic updates.
        metrics.nodes_verified_atomic.fetch_add(nodes_verified);
        metrics.steps_total_atomic.fetch_add(steps_total);
        metrics.steps_skippable_by_ast_atomic.fetch_add(steps_skippable_by_ast);
        metrics.steps_skippable_by_affine_stride_atomic.fetch_add(steps_skippable_by_affine_stride);
        metrics.steps_skippable_by_affine_stride_before_ast_atomic.fetch_add(steps_skippable_by_affine_stride_before_ast);

        // Reset everything if desired.
        if (do_reset) {
            reset();
        }
    }



    /// @brief Simple reset to set all values to zero.
    void reset() {
        nodes_verified = 0;
        steps_total = 0;
        steps_skippable_by_ast = 0;
        steps_skippable_by_affine_stride = 0;
        steps_skippable_by_affine_stride_before_ast = 0;
    }
};
