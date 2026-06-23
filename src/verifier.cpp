#include "collatz/binary_tree.hpp"
#include "collatz/verifier.hpp"



using my_t = uint64_t;



void emit(Verifier<my_t>& verifier) {
    VerifierMetric<my_t> metric = verifier.gather_metrics();
    std::cout << metric.emit_ilp() << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Must pass level to test to as arg1." << std::endl;
        return 1;
    }

    BinaryTree<my_t> tree(32);
    tree.generate_value_map();
    Verifier<my_t> verifier(tree);
    VerifierMetric<my_t> metric;
    uint64_t nodes_per_ms = 0;
    uint64_t steps_per_ms = 0;
    my_t max = my_t(1) << atoll(argv[1]);
    verifier.set_end_value(max);



    // verifier.start<true>();
    // while (verifier.get_state() != VerifierState::STOPPED) {
    //     std::this_thread::sleep_for(std::chrono::milliseconds(100));
    //     // emit(verifier);
    // }
    // std::cout << "Done (Detailed Metrics)" << std::endl;
    // // Try gathering metrics.
    // metric = verifier.gather_metrics();
    // std::cout << "I see:" << std::endl;
    // std::cout << "  Nodes Verified: " << to_string_any(metric.nodes_verified) << std::endl;
    // std::cout << "  Total Steps: " << to_string_any(metric.steps_total) << std::endl;
    // std::cout << "  Steps Skippable by High-Water Mark: " << to_string_any(metric.steps_skippable_by_hwm) << "  (rate: " << metric.skip_rate_of_hwm() << ")" << std::endl;
    // std::cout << "  Steps Skippable by Affine Striding: " << to_string_any(metric.steps_skippable_by_affine_stride) << "  (rate: " << metric.skip_rate_of_affine_stride() << ")" << std::endl;
    // std::cout << "  Steps Skippable by Affine Striding before HWM: " << to_string_any(metric.steps_skippable_by_affine_stride_before_hwm) << "  (rate: " << metric.skip_rate_of_affine_stride_before_hwm() << ")" << "  (rate of required steps:" << ((1.0 * metric.steps_skippable_by_affine_stride_before_hwm) / metric.steps_before_hwm()) << ")" << std::endl;
    // std::cout << "  Duration: " << verifier.get_duration_ms() << "ms" << std::endl;
    // nodes_per_ms = metric.nodes_verified / verifier.get_duration_ms();
    // steps_per_ms = metric.steps_total / verifier.get_duration_ms();
    // std::cout << "  Nodes Per ms: " << nodes_per_ms << std::endl;
    // std::cout << "  Steps Per ms: " << steps_per_ms << std::endl;





    verifier.start<false>();
    while (verifier.get_state() != VerifierState::STOPPED) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        emit(verifier);
    }

    std::cout << "Done (No Detailed Metrics)" << std::endl;

    // Try gathering metrics.
    metric = verifier.gather_metrics();
    std::cout << "I see:" << std::endl;
    std::cout << "  Nodes Verified: " << to_string_any(metric.nodes_verified) << std::endl;
    std::cout << "  Total Steps: " << to_string_any(metric.steps_total) << std::endl;
    std::cout << "  Steps Skippable by High-Water Mark: " << to_string_any(metric.steps_skippable_by_hwm) << "  (rate: " << metric.skip_rate_of_hwm() << ")" << std::endl;
    std::cout << "  Steps Skippable by Affine Striding: " << to_string_any(metric.steps_skippable_by_affine_stride) << "  (rate: " << metric.skip_rate_of_affine_stride() << ")" << std::endl;
    std::cout << "  Steps Skippable by Affine Striding before HWM: " << to_string_any(metric.steps_skippable_by_affine_stride_before_hwm) << "  (rate: " << metric.skip_rate_of_affine_stride_before_hwm() << ")" << "  (rate of required steps:" << ((1.0 * metric.steps_skippable_by_affine_stride_before_hwm) / metric.steps_before_hwm()) << ")" << std::endl;
    std::cout << "  Duration: " << verifier.get_duration_ms() << "ms" << std::endl;
    nodes_per_ms = metric.nodes_verified / verifier.get_duration_ms();
    steps_per_ms = metric.steps_total / verifier.get_duration_ms();
    std::cout << "  Nodes Per ms: " << nodes_per_ms << std::endl;
    std::cout << "  Steps Per ms: " << steps_per_ms << std::endl;

}
