#pragma once
#include "concepts.hpp"
#include <chrono>



/**
* @struct VerifierMetric
* @brief Basic metrics that can be gathered during verification.
* @tparam T Any supported integral (see concepts.hpp).
*/
template<AnySupportedIntegral T>
struct VerifierMetric {
    private:
    std::string _ilp;



    public:
    /// @brief Number of nodes verified.
    T nodes_verified = 0;
    /// @brief Total number of steps taken
    /// @note Only available when `DetailedMetrics` are enabled.
    T steps_total = 0;
    /// @brief Number of steps skipped (or would've been skipped) by hitting the High-Water Mark.
    /// @note Only available when `DetailedMetrics` are enabled.
    T steps_skippable_by_hwm = 0;
    /// @brief Number of steps skipped (or would've been skipped) by using Affine Strides alone.
    /// @note Only available when `DetailedMetrics` are enabled.
    T steps_skippable_by_affine_stride = 0;
    /// @brief Number of steps skipped (or would've been skipped) by using Affine Strides before hitting the HWM.
    /// @note Only available when `DetailedMetrics` are enabled.
    T steps_skippable_by_affine_stride_before_hwm = 0;
    /// @brief Duration of execution.  Usually only tracked by a Verifier, not a thread worker.
    std::chrono::milliseconds duration_ms = std::chrono::milliseconds(0);

    uint64_t nodes_per_ms() const {
        return static_cast<uint64_t>(nodes_verified / duration_ms.count());
    }

    double skip_rate_of_hwm() const {
        return (1.0 * steps_skippable_by_hwm) / steps_total;
    }

    double skip_rate_of_affine_stride() const {
        return (1.0 * steps_skippable_by_affine_stride) / steps_total;
    }

    double skip_rate_of_affine_stride_before_hwm() const {
        return (1.0 * steps_skippable_by_affine_stride_before_hwm) / steps_total;
    }

    T steps_before_hwm() const {
        return steps_total - steps_skippable_by_hwm;
    }



    /**
    * @brief Merge the members of another `Metric` object into this one.
    * @param other The other metric whose values should be added to this one.
    */
    void merge(const VerifierMetric<T>& other) {
        nodes_verified += other.nodes_verified;
        steps_total += other.steps_total;
        steps_skippable_by_hwm += other.steps_skippable_by_hwm;
        steps_skippable_by_affine_stride += other.steps_skippable_by_affine_stride;
        steps_skippable_by_affine_stride_before_hwm = other.steps_skippable_by_affine_stride_before_hwm;
        duration_ms += other.duration_ms;
    }



    /**
    * @brief Returns all metrics as Influx Line Protocol.  It does NOT emit a trailing newline.
    * @return String of all metrics.
    */
    const std::string& emit_ilp() {
        _ilp.clear();
        _ilp += "verifier ";
        _ilp += "nodes_verified=" + to_string_any(nodes_verified);
        _ilp += ",steps_total=" + to_string_any(steps_total);
        _ilp += ",steps_skippable_by_hwm=" + to_string_any(steps_skippable_by_hwm);
        _ilp += ",steps_skippable_by_affine_stride=" + to_string_any(steps_skippable_by_affine_stride);
        _ilp += ",steps_skippable_by_affine_stride_before_hwm=" + to_string_any(steps_skippable_by_affine_stride_before_hwm);
        _ilp += ",duration=" + std::to_string(duration_ms.count());
        _ilp += " " + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        return _ilp;
    }

};

