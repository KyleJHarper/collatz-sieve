#include <cassert>
#include <cstdint>
#include "../collatz/concepts.hpp"
#include "../collatz/verifier_tls_metric.hpp"
#include "../collatz/verifier_metric.hpp"
#include "helpers.hpp"




template<AnySupportedIntegral T>
void test_verifier_tls_metric_basic_members() {
    start_test(__func__);

    VerifierTLSMetric tls_metric;
    assert(tls_metric.nodes_verified == 0);
    assert(tls_metric.steps_total == 0);
    assert(tls_metric.steps_skippable_by_hwm == 0);
    assert(tls_metric.steps_skippable_by_affine_stride == 0);
    assert(tls_metric.steps_skippable_by_affine_stride_before_hwm == 0);

    end_test();
}



template<AnySupportedIntegral T>
void test_verifier_tls_metric_flush() {
    start_test(__func__);

    VerifierTLSMetric tls_metric;
    tls_metric.nodes_verified = 42;
    tls_metric.steps_total = 42;
    tls_metric.steps_skippable_by_hwm = 42;
    tls_metric.steps_skippable_by_affine_stride = 42;
    tls_metric.steps_skippable_by_affine_stride_before_hwm = 42;

    VerifierMetric flushed_metric;
    tls_metric.flush(flushed_metric, false);
    // The existing one shouldn't have been reset.
    assert(tls_metric.nodes_verified == 42);
    assert(tls_metric.steps_total == 42);
    assert(tls_metric.steps_skippable_by_hwm == 42);
    assert(tls_metric.steps_skippable_by_affine_stride == 42);
    assert(tls_metric.steps_skippable_by_affine_stride_before_hwm == 42);
    // The flushed metric should have the same values.
    assert(flushed_metric.nodes_verified_atomic.load() == 42);
    assert(flushed_metric.steps_total_atomic.load() == 42);
    assert(flushed_metric.steps_skippable_by_hwm_atomic.load() == 42);
    assert(flushed_metric.steps_skippable_by_affine_stride_atomic.load() == 42);
    assert(flushed_metric.steps_skippable_by_affine_stride_before_hwm_atomic.load() == 42);

    // Repeat the process but call reset.
    VerifierMetric flushed_metric_2;
    tls_metric.flush(flushed_metric_2, true);
    // The existing one shouldn have reset.
    assert(tls_metric.nodes_verified == 0);
    assert(tls_metric.steps_total == 0);
    assert(tls_metric.steps_skippable_by_hwm == 0);
    assert(tls_metric.steps_skippable_by_affine_stride == 0);
    assert(tls_metric.steps_skippable_by_affine_stride_before_hwm == 0);
    // The flushed metric should have the same values.
    assert(flushed_metric_2.nodes_verified_atomic.load() == 42);
    assert(flushed_metric_2.steps_total_atomic.load() == 42);
    assert(flushed_metric_2.steps_skippable_by_hwm_atomic.load() == 42);
    assert(flushed_metric_2.steps_skippable_by_affine_stride_atomic.load() == 42);
    assert(flushed_metric_2.steps_skippable_by_affine_stride_before_hwm_atomic.load() == 42);

    end_test();
}



template<AnySupportedIntegral T>
void test_verifier_tls_metric_reset() {
    start_test(__func__);

    VerifierTLSMetric tls_metric;
    tls_metric.nodes_verified = 42;
    tls_metric.steps_total = 42;
    tls_metric.steps_skippable_by_hwm = 42;
    tls_metric.steps_skippable_by_affine_stride = 42;
    tls_metric.steps_skippable_by_affine_stride_before_hwm = 42;
    // Verify the values were set.
    assert(tls_metric.nodes_verified == 42);
    assert(tls_metric.steps_total == 42);
    assert(tls_metric.steps_skippable_by_hwm == 42);
    assert(tls_metric.steps_skippable_by_affine_stride == 42);
    assert(tls_metric.steps_skippable_by_affine_stride_before_hwm == 42);
    // Reset them and assert they're 0.
    tls_metric.reset();
    assert(tls_metric.nodes_verified == 0);
    assert(tls_metric.steps_total == 0);
    assert(tls_metric.steps_skippable_by_hwm == 0);
    assert(tls_metric.steps_skippable_by_affine_stride == 0);
    assert(tls_metric.steps_skippable_by_affine_stride_before_hwm == 0);

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_verifier_tls_metric_basic_members<T>();
    test_verifier_tls_metric_flush<T>();
    test_verifier_tls_metric_reset<T>();
}



int main() {
    std::string name = "VerifierTLSMetric";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
