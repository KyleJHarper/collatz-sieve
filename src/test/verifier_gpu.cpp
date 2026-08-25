#include <cassert>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include "../collatz/concepts.hpp"
#include "../collatz/binary_tree.hpp"
#include "../collatz/verifier_gpu.hpp"
#include "helpers.hpp"



// Use a global for level count on the tree.
const level_t levels = 16;

template<FixedWidthIntegral T>
void test_verifier_gpu_basics() {
    start_test(__func__);

    // The default constructor has been deleted.  No runtime check for that.
    static_assert(!std::is_default_constructible_v<GPUVerifier<T>>, "GPUVerifier should not be default constructible.");

    // Build the tree.
    ImplicitBinaryTree<T> tree(levels);

    // Build the verifier.  This should throw because the value map isn't generated.
    try {
        GPUVerifier<T> verifier_should_fail(tree);
        assert(false);
    } catch (std::runtime_error& ex) {
        assert(std::string(ex.what()).find("You must generate the tree's value-map via generate_value_map() before building a verifier.") != std::string::npos);
    }

    // Build the verifier with a value map properly generated.
    tree.generate_value_map();
    GPUVerifier<T> verifier(tree);

    // Assert defaults.
    assert(verifier.is_iv_table_enabled() == true);
    assert(verifier.get_start_value() == 0);
    assert(verifier.get_end_value() == 0);
    assert(verifier.get_state() == VerifierState::STOPPED);
    assert(verifier.get_synchronization_countdown() == 100'000);

    // Changes to attributes should persist.
    verifier.disable_max_iv_table();
    assert(verifier.is_iv_table_enabled() == false);
    verifier.enable_max_iv_table();
    assert(verifier.is_iv_table_enabled() == true);
    verifier.set_start_value(T(42));
    assert(verifier.get_start_value() == 42);
    verifier.set_end_value(T(4200));
    assert(verifier.get_end_value() == 4200);
    verifier.set_synchronization_countdown(42);
    assert(verifier.get_synchronization_countdown() == 42);

    end_test();
}



template<FixedWidthIntegral T>
void test_verifier_gpu_run_a_range() {
    start_test(__func__);

    ImplicitBinaryTree<T> tree(levels);
    tree.generate_value_map();
    GPUVerifier<T> verifier(tree);
    verifier.set_synchronization_countdown(100);

    verifier.set_end_value(T(1) << (levels + 4));
    verifier.start();
    while(verifier.get_state() != VerifierState::STOPPED) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    const VerifierMetric& metrics = verifier.get_metrics();

    // nodes_verified
    assert(metrics.nodes_verified_atomic.load() > 0);
    // coverage_ratio and residue_ratio
    assert(metrics.coverage_ratio > 0 && metrics.coverage_ratio < 1);
    assert(metrics.residue_ratio() > 0 && metrics.residue_ratio() < 1);
    assert(metrics.coverage_ratio + metrics.residue_ratio() > 0.99);
    // effective_nodes_verified
    assert(metrics.effective_nodes_verified() > 0);
    assert(metrics.effective_nodes_verified() > metrics.nodes_verified_atomic.load());
    // steps_total (detailed metric only)
    assert(metrics.steps_total_atomic.load() == 0);
    // steps_skippable_by_ast (detailed metric only)
    assert(metrics.steps_skippable_by_ast_atomic.load() == 0);
    // steps_skippable_by_affine_stride (detailed metric only)
    assert(metrics.steps_skippable_by_affine_stride_atomic.load() == 0);
    // steps_skippable_by_affine_stride_before_ast (detailed metric only)
    assert(metrics.steps_skippable_by_affine_stride_before_ast_atomic.load() == 0);
    // gpu_kernel_launches (only on gpu verifier)
    assert(metrics.gpu_kernel_launches_atomic.load() > 0);
    // gpu_overflows_processed (shouldn't be any for this range)
    assert(metrics.gpu_overflows_processed_atomic.load() == 0);
    // gpu_overflow_buffer_exceeded (shouldn't be any for this range)
    assert(metrics.gpu_overflow_buffer_exceeded_atomic.load() == 0);
    // duration
    assert(metrics.duration_ms.count() > 0);

    end_test();
}



template<FixedWidthIntegral T>
void test_verifier_gpu_job_control() {
    start_test(__func__);

    ImplicitBinaryTree<T> tree(levels);
    tree.generate_value_map();
    GPUVerifier<T> verifier(tree);
    verifier.set_synchronization_countdown(100);
    const VerifierMetric& metrics = verifier.get_metrics();

    // Leave end value 0 to have it run forever.
    assert(verifier.get_end_value() == 0);

    // Start
    //
    // Fire it up.
    verifier.start();
    //
    // Ensure the state changed to running.
    assert(verifier.get_state() == VerifierState::RUNNING);
    //
    // Wait a bit, then try to start again.  It should return without error but keep the state the same.
    verifier.start();
    assert(verifier.get_state() == VerifierState::RUNNING);
    //
    // Calling resume() should do nothing.
    verifier.resume();

    // Pause
    //
    // Sleep for 100ms to let threads work.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    verifier.pause();
    assert(verifier.get_state() == VerifierState::PAUSED);
    //
    // Wait for workers to see PAUSED, then capture current progress.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    uint64_t nodes_verified = metrics.nodes_verified_atomic.load();
    //
    //Ensure progress was made.
    assert(nodes_verified > 0);
    //
    // Wait again.  Make sure no more nodes have been verified.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(nodes_verified == metrics.nodes_verified_atomic.load());
    //
    // Calling start() should throw an exception.
    try {
        verifier.start();
        assert(false);
    } catch (std::logic_error& ex) {
        assert(std::string(ex.what()).find("Cannot call start() unless the job is in the STOPPED state.") != std::string::npos);
    }

    // Resume
    //
    // Invoke the resume and check the state.
    verifier.resume();
    assert(verifier.get_state() == VerifierState::RUNNING);
    //
    // Wait again.  Nodes verified should go up steadily.
    for(uint64_t i = 1; i <= 5; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        assert(metrics.nodes_verified_atomic.load() > nodes_verified);
        nodes_verified = metrics.nodes_verified_atomic.load();
    }

    // Stop
    //
    // Call stop() and ensure state is set to STOPPED.  Verifier joins thread, which intrinsically sets STOPPED.
    verifier.stop();
    assert(verifier.get_state() == VerifierState::STOPPED);
    //
    // Calling pause() or resume() should throw an error.
    try {
        verifier.pause();
        assert(false);
    } catch (std::logic_error& ex) {
        assert(std::string(ex.what()).find("Cannot call pause() when a worker is STOPPED or STOPPING.") != std::string::npos);
    }
    try {
        verifier.resume();
        assert(false);
    } catch (std::logic_error& ex) {
        assert(std::string(ex.what()).find("Cannot call resume() when a worker is STOPPED or STOPPING.") != std::string::npos);
    }

    end_test();
}



template<FixedWidthIntegral T>
void run_all() {
    announce_run_all<T>();

    test_verifier_gpu_basics<T>();
    test_verifier_gpu_run_a_range<T>();
    test_verifier_gpu_job_control<T>();
}



int main() {
    std::string name = "VerifierGPU";
    preamble(name);
    if (GPU::can_use_gpu()) {
        run_all<uint64_t>();
        run_all<uint128_t>();
    } else {
        std::cout << "No GPU present.  Cannot run tests." << std::endl;
    }
    done(name);

    return 0;
}
