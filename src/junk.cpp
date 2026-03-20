#include <gmp.h>
#include <gmpxx.h>
#include <omp.h>
#include "collatz/binary_tree.hpp"


int main() {
    BinaryTreeOptions opts;
    opts.tree_type = BinaryTreeType::IMPLICIT;
    NodeBitmap<uint64_t> tree(3, opts);
}