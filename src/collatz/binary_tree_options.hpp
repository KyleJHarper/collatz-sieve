#pragma once


/**
* @struct BinaryTreeOptions
* @brief An options package for building `BinaryTree` objects.
*/
struct BinaryTreeOptions {
    /// @brief Controls pruning of High-Water Mark nodes.
    /// @note Only applies to Materialized trees.
    bool prune_hwm_nodes = false;
    /// @brief Controls pruning of parent levels, leaving only leaf nodes.
    /// @note Only applies to Materialized trees.
    bool prune_parent_levels = false;
    /// @brief Controls the collection of High-Water Mark node roots (ancestors).  These are stored in a dedicated vector.
    bool preserve_ancestors = false;
    /// @brief Controls whether verification of non-HWM nodes occurs during tree building.
    /// Required above `CollatzConstants::LARGEST_EPIRICALLY_TESTED_LEVEL`.
    bool verify_non_hwm_nodes = false;
    /// @brief Resets this options package to default values.
    void reset() {
        prune_hwm_nodes = false;
        prune_parent_levels = false;
        preserve_ancestors = false;
        verify_non_hwm_nodes = false;
    }
};
