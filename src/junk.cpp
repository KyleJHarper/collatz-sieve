#include <cuda_runtime_api.h>
#include <gmp.h>
#include <gmpxx.h>
#include "collatz/collatz.hpp"
#include "collatz/binary_tree.hpp"

int main() {
    BinaryTreeOptions opts;
    opts.tree_type = BinaryTreeType::IMPLICIT;
    BinaryTree<uint64_t> tree(32, opts);
    std::cout << Collatz<uint64_t>::st_get_step_count_fast(3) << std::endl;

}