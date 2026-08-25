#include <cassert>
#include <chrono>
#include <cstdint>
#include "../collatz/concepts.hpp"
#include "../collatz/verifier_metric.hpp"
#include "helpers.hpp"




template<AnySupportedIntegral T>
void test_verifier_metric_basic_members() {
    start_test(__func__);

    VerifierMetric metric;
    assert(metric.nodes_verified_atomic.load() == 0);
    assert(metric.steps_total_atomic.load() == 0);
    assert(metric.steps_skippable_by_ast_atomic.load() == 0);
    assert(metric.steps_skippable_by_affine_stride_atomic.load() == 0);
    assert(metric.steps_skippable_by_affine_stride_before_ast_atomic.load() == 0);
    assert(metric.gpu_kernel_launches_atomic.load() == 0);
    assert(metric.gpu_overflows_processed_atomic.load() == 0);
    assert(metric.gpu_overflow_buffer_exceeded_atomic.load() == 0);
    assert(metric.coverage_ratio == 0);
    assert(metric.residue_ratio() == 1);
    assert(metric.duration_ms == std::chrono::milliseconds(0));

    end_test();
}



template<AnySupportedIntegral T>
void test_verifier_metric_nodes_per_ms() {
    start_test(__func__);

    VerifierMetric metric;
    metric.nodes_verified_atomic.store(1000);
    metric.duration_ms = std::chrono::milliseconds(20);
    assert(metric.nodes_per_ms() == 50);

    // When ms is zero, it should return 0.
    metric.duration_ms = std::chrono::milliseconds(0);
    assert(metric.nodes_per_ms() == 0);

    end_test();
}



template<AnySupportedIntegral T>
void test_verifier_metric_effective_nodes_verified() {
    start_test(__func__);

    VerifierMetric metric;
    metric.nodes_verified_atomic.store(1000);
    metric.coverage_ratio = double(0.91);
    assert(metric.effective_nodes_verified() > 10000);

    end_test();
}



template<AnySupportedIntegral T>
void test_verifier_metric_effective_nodes_per_ms() {
    start_test(__func__);

    VerifierMetric metric;
    metric.nodes_verified_atomic.store(1000);
    metric.coverage_ratio = double(0.91);
    assert(metric.effective_nodes_verified() > 10000);
    metric.duration_ms = std::chrono::milliseconds(20);
    assert(metric.nodes_per_ms() == 50);
    assert(metric.effective_nodes_per_ms() > 500);

    // When ms is zero, it should return 0
    metric.duration_ms = std::chrono::milliseconds(0);
    assert(metric.effective_nodes_per_ms() == 0);

    end_test();
}



template<AnySupportedIntegral T>
void test_verifier_metric_steps_per_ms() {
    start_test(__func__);

    VerifierMetric metric;
    metric.steps_total_atomic.store(1000);
    metric.duration_ms = std::chrono::milliseconds(20);
    assert(metric.steps_per_ms() == 50);

    // When ms is zero, it should return 0.
    metric.duration_ms = std::chrono::milliseconds(0);
    assert(metric.steps_per_ms() == 0);

    end_test();
}



template<AnySupportedIntegral T>
void test_verifier_metric_skip_rate_of_ast() {
    start_test(__func__);

    VerifierMetric metric;
    metric.steps_skippable_by_ast_atomic.store(1001);
    metric.steps_total_atomic.store(2000);
    metric.duration_ms = std::chrono::milliseconds(20);
    assert(metric.skip_rate_of_ast() > double(0.5));

    // When step total is zero, it should return 0.
    metric.steps_total_atomic.store(0);
    assert(metric.skip_rate_of_ast() == 0);

    end_test();
}



template<AnySupportedIntegral T>
void test_verifier_metric_skip_rate_of_affine_stride() {
    start_test(__func__);

    VerifierMetric metric;
    metric.steps_skippable_by_affine_stride_atomic.store(1001);
    metric.steps_total_atomic.store(2000);
    metric.duration_ms = std::chrono::milliseconds(20);
    assert(metric.skip_rate_of_affine_stride() > double(0.5));

    // When step total is zero, it should return 0.
    metric.steps_total_atomic.store(0);
    assert(metric.skip_rate_of_affine_stride() == 0);

    end_test();
}



template<AnySupportedIntegral T>
void test_verifier_metric_skip_rate_of_affine_stride_before_ast() {
    start_test(__func__);

    VerifierMetric metric;
    metric.steps_skippable_by_affine_stride_before_ast_atomic.store(1001);
    metric.steps_total_atomic.store(2000);
    metric.duration_ms = std::chrono::milliseconds(20);
    assert(metric.skip_rate_of_affine_stride_before_ast() > double(0.5));

    // When step total is zero, it should return 0.
    metric.steps_total_atomic.store(0);
    assert(metric.skip_rate_of_affine_stride_before_ast() == 0);

    end_test();
}



template<AnySupportedIntegral T>
void test_verifier_metric_steps_before_ast() {
    start_test(__func__);

    VerifierMetric metric;
    metric.steps_total_atomic.store(1000);
    metric.steps_skippable_by_ast_atomic.store(400);
    assert(metric.steps_before_ast() == 600);

    end_test();
}



template<AnySupportedIntegral T>
void test_verifier_metric_reset() {
    start_test(__func__);

    VerifierMetric metric;

    // Start with initial values.
    assert(metric.nodes_verified_atomic.load() == 0);
    assert(metric.steps_total_atomic.load() == 0);
    assert(metric.steps_skippable_by_ast_atomic.load() == 0);
    assert(metric.steps_skippable_by_affine_stride_atomic.load() == 0);
    assert(metric.steps_skippable_by_affine_stride_before_ast_atomic.load() == 0);
    assert(metric.gpu_kernel_launches_atomic.load() == 0);
    assert(metric.gpu_overflows_processed_atomic.load() == 0);
    assert(metric.gpu_overflow_buffer_exceeded_atomic.load() == 0);
    assert(metric.coverage_ratio == 0);
    assert(metric.residue_ratio() == 1);
    assert(metric.duration_ms == std::chrono::milliseconds(0));

    // Change them all.
    metric.nodes_verified_atomic.store(42);
    metric.steps_total_atomic.store(42);
    metric.steps_skippable_by_ast_atomic.store(42);
    metric.steps_skippable_by_affine_stride_atomic.store(42);
    metric.steps_skippable_by_affine_stride_before_ast_atomic.store(42);
    metric.gpu_kernel_launches_atomic.store(42);
    metric.gpu_overflows_processed_atomic.store(42);
    metric.gpu_overflow_buffer_exceeded_atomic.store(42);
    metric.duration_ms = std::chrono::milliseconds(42);

    // Ensure they stay changed.
    assert(metric.nodes_verified_atomic.load() == 42);
    assert(metric.steps_total_atomic.load() == 42);
    assert(metric.steps_skippable_by_ast_atomic.load() == 42);
    assert(metric.steps_skippable_by_affine_stride_atomic.load() == 42);
    assert(metric.steps_skippable_by_affine_stride_before_ast_atomic.load() == 42);
    assert(metric.gpu_kernel_launches_atomic.load() == 42);
    assert(metric.gpu_overflows_processed_atomic.load() == 42);
    assert(metric.gpu_overflow_buffer_exceeded_atomic.load() == 42);
    assert(metric.coverage_ratio == 0);
    assert(metric.residue_ratio() == 1);
    assert(metric.duration_ms == std::chrono::milliseconds(42));

    // Reset them.
    metric.reset();

    // Check they are back to initial values.
    assert(metric.nodes_verified_atomic.load() == 0);
    assert(metric.steps_total_atomic.load() == 0);
    assert(metric.steps_skippable_by_ast_atomic.load() == 0);
    assert(metric.steps_skippable_by_affine_stride_atomic.load() == 0);
    assert(metric.steps_skippable_by_affine_stride_before_ast_atomic.load() == 0);
    assert(metric.gpu_kernel_launches_atomic.load() == 0);
    assert(metric.gpu_overflows_processed_atomic.load() == 0);
    assert(metric.gpu_overflow_buffer_exceeded_atomic.load() == 0);
    // Coverage and residue ratios do not change with reset().
    assert(metric.coverage_ratio == 0);
    assert(metric.residue_ratio() == 1);
    assert(metric.duration_ms == std::chrono::milliseconds(0));

    end_test();
}



template<AnySupportedIntegral T>
void test_verifier_metric_emit_ilp() {
    start_test(__func__);

    VerifierMetric metric;
    metric.nodes_verified_atomic.store(42);
    metric.steps_total_atomic.store(42);
    metric.steps_skippable_by_ast_atomic.store(42);
    metric.steps_skippable_by_affine_stride_atomic.store(42);
    metric.steps_skippable_by_affine_stride_before_ast_atomic.store(42);
    metric.gpu_kernel_launches_atomic.store(42);
    metric.gpu_overflows_processed_atomic.store(42);
    metric.gpu_overflow_buffer_exceeded_atomic.store(42);
    metric.duration_ms = std::chrono::milliseconds(42);
    metric.coverage_ratio = double(0.91);

    // Now string check.
    std::string ilp = metric.emit_ilp();
    assert(ilp.find("verifier ") != std::string::npos);
    assert(ilp.find(" nodes_verified=42") != std::string::npos);
    assert(ilp.find(",coverage_ratio=0.91") != std::string::npos);
    assert(ilp.find(",effective_nodes_verified=466") != std::string::npos);
    assert(ilp.find(",steps_total=42") != std::string::npos);
    assert(ilp.find(",steps_skippable_by_ast=42") != std::string::npos);
    assert(ilp.find(",steps_skippable_by_affine_stride=42") != std::string::npos);
    assert(ilp.find(",steps_skippable_by_affine_stride_before_ast=42") != std::string::npos);
    assert(ilp.find(",gpu_kernel_launches=42") != std::string::npos);
    assert(ilp.find(",gpu_overflows_processed=42") != std::string::npos);
    assert(ilp.find(",gpu_overflow_buffer_exceeded=42") != std::string::npos);
    assert(ilp.find(",duration_ms=42 ") != std::string::npos);

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_verifier_metric_basic_members<T>();
    test_verifier_metric_nodes_per_ms<T>();
    test_verifier_metric_effective_nodes_verified<T>();
    test_verifier_metric_effective_nodes_per_ms<T>();
    test_verifier_metric_steps_per_ms<T>();
    test_verifier_metric_skip_rate_of_ast<T>();
    test_verifier_metric_skip_rate_of_affine_stride<T>();
    test_verifier_metric_skip_rate_of_affine_stride_before_ast<T>();
    test_verifier_metric_steps_before_ast<T>();
    test_verifier_metric_reset<T>();
    test_verifier_metric_emit_ilp<T>();
}



int main() {
    std::string name = "VerifierMetric";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
