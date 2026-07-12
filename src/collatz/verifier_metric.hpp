#pragma once
#include <atomic>
#include "string.hpp"
#include <chrono>






/**
* @struct VerifierMetrics
* @brief Basic metrics that can be gathered during verification.
*/
struct VerifierMetric {
    public:
    /// @brief Number of nodes verified.
    std::atomic<uint64_t> nodes_verified_atomic = 0;



    /// @brief Total number of steps taken
    /// @note Only available when `DetailedMetrics` are enabled.
    std::atomic<uint64_t> steps_total_atomic = 0;



    /// @brief Number of steps skipped (or would've been skipped) by hitting the High-Water Mark.
    /// @note Only available when `DetailedMetrics` are enabled.
    std::atomic<uint64_t> steps_skippable_by_hwm_atomic = 0;



    /// @brief Number of steps skipped (or would've been skipped) by using Affine Strides alone.
    /// @note Only available when `DetailedMetrics` are enabled.
    std::atomic<uint64_t> steps_skippable_by_affine_stride_atomic = 0;



    /// @brief Number of steps skipped (or would've been skipped) by using Affine Strides before hitting the HWM.
    /// @note Only available when `DetailedMetrics` are enabled.
    std::atomic<uint64_t> steps_skippable_by_affine_stride_before_hwm_atomic = 0;



    /// @brief Number of GPU kernel launches.
    std::atomic<uint64_t> gpu_kernel_launches = 0;



    /// @brief Number of values which triggered overflow on the GPU and required follow-up CPU verification.
    std::atomic<uint64_t> gpu_overflows_processed = 0;



    /// @brief Number of times overflows became so high they exceeded the `GPUVerifierResultData` overflow indexes buffer.
    std::atomic<uint64_t> gpu_overflow_buffer_exceeded = 0;



    /// @brief User-supplied coverage ratio, usually provided by the `tree`.
    double coverage_ratio = 0;



    /// @brief Returns the residue ratio, which is (1 - coverage_ratio).
    double residue_ratio() const {
        return 1 - coverage_ratio;
    }



    /// @brief The cumulative duration in milliseconds this verifier has ran.
    std::chrono::milliseconds duration_ms = std::chrono::milliseconds(0);



    /// @brief Number of nodes verified per millisecond.
    /// @note Data is only updated every `Verifier::_synchronization_countdown` iterations.
    uint64_t nodes_per_ms(std::memory_order m = std::memory_order_relaxed) const {
        return static_cast<uint64_t>(nodes_verified_atomic.load(m) / duration_ms.count());
    }



    /// @brief Number of nodes verified per millisecond.
    /// @note Data is only updated every `Verifier::_synchronization_countdown` iterations.
    uint64_t effective_nodes_per_ms() const {
        return effective_nodes_verified() / duration_ms.count();
    }



    /// @brief Number of steps verified per millisecond.
    /// @note Data is only updated every `Verifier::_synchronization_countdown` iterations.
    uint64_t steps_per_ms(std::memory_order m = std::memory_order_relaxed) const {
        return static_cast<uint64_t>(steps_total_atomic.load(m) / duration_ms.count());
    }



    /// @brief Rate of steps skipped by using High-Water Mark stopping.
    /// @note Data is only updated every `Verifier::_synchronization_countdown` iterations.
    /// @note Only available when `DetailedMetrics` are enabled.
    double skip_rate_of_hwm(std::memory_order m = std::memory_order_relaxed) const {
        return (1.0 * steps_skippable_by_hwm_atomic.load(m)) / steps_total_atomic.load(m);
    }



    /// @brief Rate of steps skipped by using `AffineStride` tables without using High-Water Mark stopping.
    /// @note Data is only updated every `Verifier::_synchronization_countdown` iterations.
    /// @note Only available when `DetailedMetrics` are enabled.
    double skip_rate_of_affine_stride(std::memory_order m = std::memory_order_relaxed) const {
        return (1.0 * steps_skippable_by_affine_stride_atomic.load(m)) / steps_total_atomic.load(m);
    }



    /// @brief Rate of steps skipped by using `AffineStride` tables while using High-Water Mark stopping.
    /// @note Data is only updated every `Verifier::_synchronization_countdown` iterations.
    /// @note Only available when `DetailedMetrics` are enabled.
    double skip_rate_of_affine_stride_before_hwm(std::memory_order m = std::memory_order_relaxed) const {
        return (1.0 * steps_skippable_by_affine_stride_before_hwm_atomic.load(m)) / steps_total_atomic.load(m);
    }



    /// @brief Number of steps required to reach High-Water Mark.
    /// @note Data is only updated every `Verifier::_synchronization_countdown` iterations.
    /// @note Only available when `DetailedMetrics` are enabled.
    uint64_t steps_before_hwm(std::memory_order m = std::memory_order_relaxed) const {
        return steps_total_atomic.load(m) - steps_skippable_by_hwm_atomic.load(m);
    }



    /// @brief Returns the effective nodes processed processed, which is true nodes divided by (1 - coverage ratio).
    uint64_t effective_nodes_verified() const {
        return nodes_verified_atomic.load(std::memory_order_relaxed)  / residue_ratio();
    }



    /// @brief Simple reset to set all values to zero.
    void reset() {
        nodes_verified_atomic.store(0);
        // This should not change.  It's tied to a tree at instantiation.  coverage_ratio = 0;
        steps_total_atomic.store(0);
        steps_skippable_by_hwm_atomic.store(0);
        steps_skippable_by_affine_stride_atomic.store(0);
        steps_skippable_by_affine_stride_before_hwm_atomic.store(0);
        gpu_kernel_launches.store(0);
        duration_ms = std::chrono::milliseconds(0);
    }



    /**
    * @brief Returns all metrics as Influx Line Protocol.  It does NOT emit a trailing newline.
    * @return String of all metrics.
    */
    const std::string emit_ilp(std::memory_order m = std::memory_order_relaxed) const {
        std::string ilp;
        ilp.clear();
        ilp += "verifier ";
        ilp += "nodes_verified=" + to_string_any(nodes_verified_atomic.load(m));
        ilp += ",coverage_ratio=" + std::to_string(coverage_ratio);
        ilp += ",effective_nodes_verified=" + to_string_any(effective_nodes_verified());
        ilp += ",steps_total=" + to_string_any(steps_total_atomic.load(m));
        ilp += ",steps_skippable_by_hwm=" + to_string_any(steps_skippable_by_hwm_atomic.load(m));
        ilp += ",steps_skippable_by_affine_stride=" + to_string_any(steps_skippable_by_affine_stride_atomic.load(m));
        ilp += ",steps_skippable_by_affine_stride_before_hwm=" + to_string_any(steps_skippable_by_affine_stride_before_hwm_atomic.load(m));
        ilp += ",gpu_kernel_launches=" + to_string_any(gpu_kernel_launches.load(m));
        ilp += ",gpu_overflows_processed=" + to_string_any(gpu_overflows_processed.load(m));
        ilp += ",gpu_overflow_buffer_exceeded=" + to_string_any(gpu_overflow_buffer_exceeded.load(m));
        ilp += ",duration=" + std::to_string(duration_ms.count());
        ilp += " " + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        return ilp;
    }

};
