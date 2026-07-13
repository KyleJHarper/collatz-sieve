#include "collatz/binary_tree.hpp"
// #include "collatz/verifier_gpu.hpp"
#include "collatz/verifier_cpu.hpp"
#include "collatz/verifier_executor_policy.hpp"
#include <thread>


int main() {
    // Pick a type.
    using my_t = uint64_t;
    // using my_t = mpz_class;  // Only on the CPU path.  GPU can't do GMP.

    // Make the tree and generate the value map.  Or load one from save.
    level_t levels = 32;
    BinaryTree<my_t> tree(levels);
    tree.generate_value_map();

    // Pick a verifier.
    CPUVerifier<my_t> verifier(tree);
    // GPUVerifier<my_t> verifier(tree);  // Use the GPU Verifier if you have a compatible RTX card.

    // Set a max value to test do.  In this case, we'll stop at 2^40 since the tree stopped at 2^32.
    my_t max = my_t(1) << 40;
    verifier.set_end_value(max);

    // Execution policy decides things like whether detailed metrics should be calculated for debugging (CPU only).
    VerifierExecutorPolicy policy;
    policy.detailed_metrics = false;
    // For CPU, the IV table is faster.  For GPU, it isn't.
    policy.enable_max_iv_table = true;
    // If using GPU, the scales_per_run is a powerful amortization feature.  Values of 100-10,000 are common.
    policy.scales_per_run = 1000;

    // Start the verifier and wait for it to finish, checking metrics and emitting them in Influx Line Protocol format.
    verifier.start(policy);
    while (verifier.get_state() != VerifierState::STOPPED) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << verifier.get_metrics().emit_ilp() << std::endl;
    }

    // Pull the latest metrics and spit it all out.
    const VerifierMetric& metric = verifier.get_metrics();
    std::cout << "  Nodes Verified: " << to_string_any(metric.nodes_verified_atomic.load()) << std::endl;
    std::cout << "  Total Steps: " << to_string_any(metric.steps_total_atomic.load()) << std::endl;
    std::cout << "  Steps Skippable by High-Water Mark: " << to_string_any(metric.steps_skippable_by_hwm_atomic.load()) << "  (rate: " << metric.skip_rate_of_hwm() << ")" << std::endl;
    std::cout << "  Steps Skippable by Affine Striding: " << to_string_any(metric.steps_skippable_by_affine_stride_atomic.load()) << "  (rate: " << metric.skip_rate_of_affine_stride() << ")" << std::endl;
    std::cout << "  Steps Skippable by Affine Striding before HWM: " << to_string_any(metric.steps_skippable_by_affine_stride_before_hwm_atomic.load()) << "  (rate: " << metric.skip_rate_of_affine_stride_before_hwm() << ")" << "  (rate of required steps:" << ((1.0 * metric.steps_skippable_by_affine_stride_before_hwm_atomic.load()) / metric.steps_before_hwm()) << ")" << std::endl;
    std::cout << "  Duration: " << metric.duration_ms.count() << "ms" << std::endl;
    std::cout << "  Nodes Per ms: " << metric.nodes_per_ms() << std::endl;
    std::cout << "  Steps Per ms: " << metric.steps_per_ms() << std::endl;
    std::cout << "  Effective Nodes Verified: " << metric.effective_nodes_verified() << std::endl;
    std::cout << "  Effective Nodes Per ms: " << metric.effective_nodes_per_ms() << std::endl;
}
