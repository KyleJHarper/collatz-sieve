#include <iostream>
#include "collatz/binary_tree.hpp"

int main() {

    BinaryTreeOptions opts_with_no_pruning;
    opts_with_no_pruning.prune_hwm_nodes = false;
    opts_with_no_pruning.prune_parent_levels = false;
    BinaryTreeOptions opts_with_hwm_pruning;
    opts_with_hwm_pruning.prune_hwm_nodes = true;
    opts_with_hwm_pruning.prune_parent_levels = false;
    BinaryTreeOptions opts_with_parent_pruning;
    opts_with_parent_pruning.prune_hwm_nodes = false;
    opts_with_parent_pruning.prune_parent_levels = true;
    BinaryTreeOptions opts_with_all_pruning;
    opts_with_all_pruning.prune_hwm_nodes = true;
    opts_with_all_pruning.prune_parent_levels = true;

    // Materialized
    level_t levels = 8;
    MaterializedBinaryTree<uint64_t> tree_with_no_pruning(levels, opts_with_no_pruning);
    MaterializedBinaryTree<uint64_t> tree_with_hwm_pruning(levels, opts_with_hwm_pruning);
    MaterializedBinaryTree<uint64_t> tree_with_parent_pruning(levels, opts_with_parent_pruning);
    MaterializedBinaryTree<uint64_t> tree_with_all_pruning(levels, opts_with_all_pruning);

    std::cout << "tree_with_no_pruning=" << tree_with_no_pruning.real_node_count() << std::endl;
    std::cout << "tree_with_hwm_pruning=" << tree_with_hwm_pruning.real_node_count() << std::endl;
    std::cout << "tree_with_parent_pruning=" << tree_with_parent_pruning.real_node_count() << std::endl;
    std::cout << "tree_with_all_pruning=" << tree_with_all_pruning.real_node_count() << std::endl;

}
