#include <cassert>
#include <cstdint>
#include "../collatz/concepts.hpp"
#include "../collatz/verifier_gpu_result_data.hpp"
#include "helpers.hpp"




template<AnySupportedIntegral T>
void test_verifier_gpu_result_data_basic_members() {
    start_test(__func__);

    GPUVerifierResultData results;
    assert(results.MAX_OVERFLOW == 4096);
    assert(results.overflow_count == 0);
    assert(results.overflow_exceeded == false);

    end_test();
}



template<AnySupportedIntegral T>
void test_verifier_gpu_result_data_reset() {
    start_test(__func__);

    GPUVerifierResultData results;

    // Should have defaults.
    assert(results.MAX_OVERFLOW == 4096);
    assert(results.overflow_count == 0);
    assert(results.overflow_exceeded == false);

    // Change them.
    results.overflow_count = 42;
    results.overflow_exceeded = true;

    // They should stay changed.
    assert(results.MAX_OVERFLOW == 4096);
    assert(results.overflow_count == 42);
    assert(results.overflow_exceeded == true);

    // Reset
    results.reset();

    // They should be default again.
    assert(results.MAX_OVERFLOW == 4096);
    assert(results.overflow_count == 0);
    assert(results.overflow_exceeded == false);

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_verifier_gpu_result_data_basic_members<T>();
    test_verifier_gpu_result_data_reset<T>();
}



int main() {
    std::string name = "VerifierGPUResultData";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
