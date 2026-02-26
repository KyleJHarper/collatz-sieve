#include <cuda_runtime_api.h>
#include <gmp.h>
#include <gmpxx.h>
#include <omp.h>
#include "collatz/binary_tree.hpp"


int main(int argc, char* argv[]) {
    std::cout << "arg count: " << argc << std::endl;
    size_t levels = atoi(argv[1]);
    // omp_set_num_threads(1);
    BinaryTreeMath<uint64_t>::set_root_value(0);
    BinaryTreeOptions opts;
    opts.tree_type = BinaryTreeType::IMPLICIT;
    BinaryTree<uint64_t> tree(levels, opts);
    std::cout << "Uncovered intervals are:" << std::endl;
    for (const Interval<uint64_t>& interval : tree.get_uncovered_intervals()) {
        std::cout << "  start=" << to_string_any(interval.start) << ", end=" << to_string_any(interval.end) << std::endl;
    }

}