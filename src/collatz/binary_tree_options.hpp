#pragma once


//
// Options Package for BinaryTree
//
struct BinaryTreeOptions {
    bool prune_hwm_nodes = false;
    bool prune_parent_levels = false;
    bool preserve_ancestors = false;
    bool verify_non_hwm_nodes = false;
    void reset() {
        prune_hwm_nodes = false;
        prune_parent_levels = false;
        preserve_ancestors = false;
        verify_non_hwm_nodes = false;
    }
};
