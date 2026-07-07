#include "collatz/binary_tree.hpp"
#include "collatz/verifier_gpu.hpp"
#include "collatz/verifier_cpu.hpp"
#include "collatz/verifier_executor_policy.hpp"



using my_t = uint64_t;
// using my_t = mpz_class;



int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Must pass level to test to as arg1." << std::endl;
        return 1;
    }

    level_t levels = 32;
    BinaryTree<my_t> tree(levels);
    tree.generate_value_map();
    GPUVerifier<my_t> verifier(tree);
    // CPUVerifier<my_t> verifier(tree);
    uint64_t effective_nodes = 0;
    uint64_t effective_nodes_per_ms =  0;
    my_t min = my_t(1) << levels;
    my_t max = my_t(1) << atoll(argv[1]);
    verifier.set_end_value(max);

    VerifierExecutorPolicy policy;
    policy.detailed_metrics = false;
    // policy.enable_max_iv_table = true;
    policy.enable_max_iv_table = false;
    verifier.start(policy);
    while (verifier.get_state() != VerifierState::STOPPED) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << verifier.get_metrics().emit_ilp() << std::endl;
    }

    std::cout << "Done (No Detailed Metrics)" << std::endl;

    // Try gathering metrics.
    const VerifierMetric& metric = verifier.get_metrics();
    std::cout << "I see:" << std::endl;
    std::cout << "  Nodes Verified: " << to_string_any(metric.nodes_verified_atomic.load()) << std::endl;
    std::cout << "  Total Steps: " << to_string_any(metric.steps_total_atomic.load()) << std::endl;
    std::cout << "  Steps Skippable by High-Water Mark: " << to_string_any(metric.steps_skippable_by_hwm_atomic.load()) << "  (rate: " << metric.skip_rate_of_hwm() << ")" << std::endl;
    std::cout << "  Steps Skippable by Affine Striding: " << to_string_any(metric.steps_skippable_by_affine_stride_atomic.load()) << "  (rate: " << metric.skip_rate_of_affine_stride() << ")" << std::endl;
    std::cout << "  Steps Skippable by Affine Striding before HWM: " << to_string_any(metric.steps_skippable_by_affine_stride_before_hwm_atomic.load()) << "  (rate: " << metric.skip_rate_of_affine_stride_before_hwm() << ")" << "  (rate of required steps:" << ((1.0 * metric.steps_skippable_by_affine_stride_before_hwm_atomic.load()) / metric.steps_before_hwm()) << ")" << std::endl;
    std::cout << "  Duration: " << metric.duration_ms.count() << "ms" << std::endl;
    effective_nodes = max - min;
    // effective_nodes = max.get_ui() - min.get_ui();
    effective_nodes_per_ms = effective_nodes / metric.duration_ms.count();
    std::cout << "  Nodes Per ms: " << metric.nodes_per_ms() << std::endl;
    std::cout << "  Steps Per ms: " << metric.steps_per_ms() << std::endl;
    std::cout << "  Effective Nodes Verified: " << effective_nodes << std::endl;
    std::cout << "  Effective Nodes Per ms: " << effective_nodes_per_ms << std::endl;

}
