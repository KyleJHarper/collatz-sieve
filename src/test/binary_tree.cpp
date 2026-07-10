#include <cassert>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <gmpxx.h>
#include <omp.h>
#include <stdexcept>
#include "helpers.hpp"
#include "../collatz/binary_tree.hpp"
#include "../collatz/binary_tree_coverage_constants.hpp"




template<AnySupportedIntegral T>
void test_binary_tree_basic_construction() {
    start_test(__func__);

    // Tree should be implicit by default.
    BinaryTree<T> tree_implicit_by_default(5);
    assert(tree_implicit_by_default.is_implicit());

    // Make a materialized one and do some tests.
    MaterializedBinaryTree<T> tree(5);
    assert(tree.is_materialized());

    // Build extras to ensure isolation, double-freeing, etc are all good.  These will destruct at function end (end of scope).
    MaterializedBinaryTree<T> tree2(2);
    MaterializedBinaryTree<T> tree3(4);
    MaterializedBinaryTree<T> tree4(5);
    MaterializedBinaryTree<T> tree5(6);
    MaterializedBinaryTree<T> tree6(7);

    // Aliases should work.
    MaterializedBinaryTree<T> x(1);
    assert(x.is_materialized());
    ImplicitBinaryTree<T> y(1);
    assert(y.is_implicit());

    // Empty trees should be okay.
    ImplicitBinaryTree<T> zero_implicit_tree;
    assert(zero_implicit_tree.get_level_count() == 0);
    assert(zero_implicit_tree.is_initialized() == false);
    MaterializedBinaryTree<T> zero_materialized_tree;
    assert(zero_materialized_tree.get_level_count() == 0);
    assert(zero_materialized_tree.is_initialized() == false);

    // Adding levels to them should be fine, and should flag initialization.
    zero_implicit_tree.add_level();
    zero_materialized_tree.add_level();
    assert(zero_implicit_tree.get_level_count() == 1);
    assert(zero_materialized_tree.get_level_count() == 1);
    assert(zero_implicit_tree.is_initialized() == true);
    assert(zero_materialized_tree.is_initialized() == true);

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_init() {
    start_test(__func__);

    // Make a tree.
    BinaryTree<T> tree(5);
    assert(tree.get_level_count() == 5);

    // Shrink it.
    tree.init(3);
    assert(tree.get_level_count() == 3);

    // Add a level.
    tree.add_level();
    assert(tree.get_level_count() == 4);

    // Grow it.
    tree.init(16);
    assert(tree.get_level_count() == 16);

    // Start from an uninitialized tree.
    // Implicit
    ImplicitBinaryTree<T> uninitialized_implicit_tree;
    assert(uninitialized_implicit_tree.is_initialized() == false);
    uninitialized_implicit_tree.init(14);
    assert(uninitialized_implicit_tree.get_level_count() == 14);
    assert(uninitialized_implicit_tree.is_initialized() == true);
    // Materialized
    MaterializedBinaryTree<T> uninitialized_materialized_tree;
    assert(uninitialized_materialized_tree.is_initialized() == false);
    uninitialized_materialized_tree.init(14);
    assert(uninitialized_materialized_tree.get_level_count() == 14);
    assert(uninitialized_materialized_tree.is_initialized() == true);

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_reset() {
    start_test(__func__);

    ImplicitBinaryTree<T> implicit_tree(6);
    MaterializedBinaryTree<T> materialized_tree(6);

    assert(implicit_tree.get_level_count() == 6);
    assert(materialized_tree.get_level_count() == 6);
    assert(implicit_tree.is_initialized() == true);
    assert(materialized_tree.is_initialized() == true);

    // Now reset.
    implicit_tree.reset();
    materialized_tree.reset();
    assert(implicit_tree.get_level_count() == 0);
    assert(materialized_tree.get_level_count() == 0);
    assert(implicit_tree.is_initialized() == false);
    assert(materialized_tree.is_initialized() == false);

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_get_impl() {
    start_test(__func__);

    // Implicit
    BinaryTree<T, BinaryTreeImplicitImpl<T>> implicit_tree(8);
    const BinaryTreeImplicitImpl<T>& implicit_impl = implicit_tree.get_impl();
    assert(implicit_impl.is_implicit());

    // Materialized
    BinaryTree<T, BinaryTreeMaterializedImpl<T>> materialized_tree(8);
    const BinaryTreeMaterializedImpl<T>& materialized_impl = materialized_tree.get_impl();
    assert(materialized_impl.is_materialized());

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_get_tree_type() {
    start_test(__func__);

    // Implicit
    BinaryTree<T, BinaryTreeImplicitImpl<T>> implicit_tree(8);
    assert(implicit_tree.get_tree_type() == TreeTypeEnum::IMPLICIT);

    // Materialized
    BinaryTree<T, BinaryTreeMaterializedImpl<T>> materialized_tree(8);
    assert(materialized_tree.get_tree_type() == TreeTypeEnum::MATERIALIZED);

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_get_level_count() {
    start_test(__func__);

    level_t max_level = 16;
    for (level_t level = 1; level <= max_level; level++) {
        // Implicit
        ImplicitBinaryTree<T> implicit_tree(level);
        assert(implicit_tree.get_level_count() == level);
        implicit_tree.add_level();
        assert(implicit_tree.get_level_count() == level + 1);

        // Materialized
        MaterializedBinaryTree<T> materialized_tree(level);
        assert(materialized_tree.get_level_count() == level);
        materialized_tree.add_level();
        assert(materialized_tree.get_level_count() == level + 1);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_get_scaling_factor() {
    start_test(__func__);

    level_t max_level = 16;
    for (level_t level = 1; level <= max_level; level++) {
        if (level < 2) {
            try {
                T x = BinaryTreeMath<T>::st_scaling_factor(level);
                assert(false);
                x++;
            } catch (std::out_of_range& e) {
                assert(std::string(e.what()).find("Cannot request a scaling factor for levels below 2.") != std::string::npos);
            }
            continue;
        }
        T scaling_factor = T{1} << (level - 2);
        assert(scaling_factor == BinaryTreeMath<T>::st_scaling_factor(level));
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_get_root_node() {
    start_test(__func__);

    ImplicitBinaryTree<T> implicit_tree;
    assert(implicit_tree.get_root_node() == nullptr);
    implicit_tree.init(4);
    assert(implicit_tree.get_root_node() != nullptr);
    assert(implicit_tree.get_root_node()->get_value() == BinaryTreeMath<T>::get_root_value());

    MaterializedBinaryTree<T> materialized_tree;
    assert(materialized_tree.get_root_node() == nullptr);
    materialized_tree.init(4);
    assert(materialized_tree.get_root_node() != nullptr);
    assert(materialized_tree.get_root_node()->get_value() == BinaryTreeMath<T>::get_root_value());

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_get_coverage_map() {
    start_test(__func__);

    level_t max_level = 4;

    // Implicit
    ImplicitBinaryTree<T> implicit_tree(max_level);
    const std::unordered_map<level_t, BinaryTreeCoverage<T>>& implicit_coverage_map = implicit_tree.get_coverage_map();
    assert(implicit_coverage_map.size() == max_level);
    for (level_t level = 1; level <= max_level; level++) {
        const BinaryTreeCoverage<T>& coverage = implicit_coverage_map.at(level);
        assert(coverage.get_covered() == BinaryTreeCoverageConstants::get_known_coverage<T>(level));
        assert(coverage.get_total() == BinaryTreeCoverageConstants::get_total<T>(level));
    }

    // Materialized
    MaterializedBinaryTree<T> materialized_tree(max_level);
    const std::unordered_map<level_t, BinaryTreeCoverage<T>>& materialized_coverage_map = materialized_tree.get_coverage_map();
    assert(materialized_coverage_map.size() == max_level);
    for (level_t level = 1; level <= max_level; level++) {
        const BinaryTreeCoverage<T>& coverage = materialized_coverage_map.at(level);
        assert(coverage.get_covered() == BinaryTreeCoverageConstants::get_known_coverage<T>(level));
        assert(coverage.get_total() == BinaryTreeCoverageConstants::get_total<T>(level));
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_get_ancestors() {
    start_test(__func__);

    level_t max_level = 5;
    BinaryTreeOptions opts_without_ancestors;
    opts_without_ancestors.preserve_ancestors = false;
    BinaryTreeOptions opts_with_ancestors;
    opts_with_ancestors.preserve_ancestors = true;

    // Implicit
    ImplicitBinaryTree<T> implicit_tree_without_ancestors(max_level, opts_without_ancestors);
    assert(implicit_tree_without_ancestors.get_ancestors().size() == 0);
    ImplicitBinaryTree<T> implicit_tree_with_ancestors(max_level, opts_with_ancestors);
    assert(implicit_tree_with_ancestors.get_ancestors().size() > 0);
    assert(implicit_tree_with_ancestors.get_ancestors()[0]->get_value() == 2);
    assert(implicit_tree_with_ancestors.get_ancestors()[1]->get_value() == 5);
    assert(implicit_tree_with_ancestors.get_ancestors()[2]->get_value() == 19);

    // Materialized
    MaterializedBinaryTree<T> materialized_tree_without_ancestors(max_level, opts_without_ancestors);
    assert(materialized_tree_without_ancestors.get_ancestors().size() == 0);
    MaterializedBinaryTree<T> materialized_tree_with_ancestors(max_level, opts_with_ancestors);
    assert(materialized_tree_with_ancestors.get_ancestors().size() > 0);
    assert(materialized_tree_with_ancestors.get_ancestors()[0]->get_value() == 2);
    assert(materialized_tree_with_ancestors.get_ancestors()[1]->get_value() == 5);
    assert(materialized_tree_with_ancestors.get_ancestors()[2]->get_value() == 19);

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_is_verifying_non_hwm_nodes() {
    start_test(__func__);

    // Implicit
    ImplicitBinaryTree<T> implicit_tree;
    assert(implicit_tree.is_verifying_non_hwm_nodes() == false);
    implicit_tree.enable_non_hwm_node_verification();
    assert(implicit_tree.is_verifying_non_hwm_nodes() == true);
    implicit_tree.disable_non_hwm_node_verification();
    assert(implicit_tree.is_verifying_non_hwm_nodes() == false);

    // Materialized
    MaterializedBinaryTree<T> materialized_tree;
    assert(materialized_tree.is_verifying_non_hwm_nodes() == false);
    materialized_tree.enable_non_hwm_node_verification();
    assert(materialized_tree.is_verifying_non_hwm_nodes() == true);
    materialized_tree.disable_non_hwm_node_verification();
    assert(materialized_tree.is_verifying_non_hwm_nodes() == false);

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_node_count() {
    start_test(__func__);

    level_t levels = 8;

    ImplicitBinaryTree<T> implicit_tree(levels);
    assert(implicit_tree.node_count() == BinaryTreeMath<T>::st_node_count_of_tree(levels));

    MaterializedBinaryTree<T> materialized_tree(levels);
    assert(materialized_tree.node_count() == BinaryTreeMath<T>::st_node_count_of_tree(levels));

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_materialized_level_map() {
    start_test(__func__);

    // Materialized
    MaterializedBinaryTree<T> tree(5);
    const auto& map = tree.get_level_map();
    size_t level = 0;
    //
    // Level 1: root
    level = 1;
    assert(map.at(level).size() == 1);
    assert(map.at(level)[0]->get_value() == 0 + BinaryTreeMath<T>::get_root_value());
    //
    // Level 2: 2 children (0 + 1 + Offset, 0 + 2 + Offset)
    level = 2;
    assert(map.at(level).size() == 2);
    assert(map.at(level)[0]->get_value() == 1 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[1]->get_value() == 2 + BinaryTreeMath<T>::get_root_value());
    //
    // Level 3: each child has 2 children, total 4
    level = 3;
    assert(map.at(level).size() == 4);
    assert(map.at(level)[0]->get_value() == 3 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[1]->get_value() == 5 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[2]->get_value() == 4 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[3]->get_value() == 6 + BinaryTreeMath<T>::get_root_value());
    //
    // Level 4: 8 nodes expected
    level = 4;
    assert(map.at(level).size() == 8);
    assert(map.at(level)[0]->get_value() == 7 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[1]->get_value() == 11 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[2]->get_value() == 9 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[3]->get_value() == 13 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[4]->get_value() == 8 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[5]->get_value() == 12 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[6]->get_value() == 10 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[7]->get_value() == 14 + BinaryTreeMath<T>::get_root_value());
    //
    // Level 5: 16 nodes expected
    level = 5;
    assert(map.at(level).size() == 16);
    assert(map.at(level)[0]->get_value() == 15 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[1]->get_value() == 23 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[2]->get_value() == 19 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[3]->get_value() == 27 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[4]->get_value() == 17 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[5]->get_value() == 25 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[6]->get_value() == 21 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[7]->get_value() == 29 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[8]->get_value() == 16 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[9]->get_value() == 24 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[10]->get_value() == 20 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[11]->get_value() == 28 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[12]->get_value() == 18 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[13]->get_value() == 26 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[14]->get_value() == 22 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(level)[15]->get_value() == 30 + BinaryTreeMath<T>::get_root_value());

    end_test();
}


template<AnySupportedIntegral T>
void test_binary_tree_materialized_pruning_options() {
    start_test(__func__);

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
    MaterializedBinaryTree<T> tree_with_no_pruning(levels, opts_with_no_pruning);
    assert(tree_with_no_pruning.is_pruning_hwm_nodes() == false);
    assert(tree_with_no_pruning.is_pruning_parent_levels() == false);
    MaterializedBinaryTree<T> tree_with_hwm_pruning(levels, opts_with_hwm_pruning);
    assert(tree_with_hwm_pruning.is_pruning_hwm_nodes() == true);
    assert(tree_with_hwm_pruning.is_pruning_parent_levels() == false);
    MaterializedBinaryTree<T> tree_with_parent_pruning(levels, opts_with_parent_pruning);
    assert(tree_with_parent_pruning.is_pruning_hwm_nodes() == false);
    assert(tree_with_parent_pruning.is_pruning_parent_levels() == true);
    MaterializedBinaryTree<T> tree_with_all_pruning(levels, opts_with_all_pruning);
    assert(tree_with_all_pruning.is_pruning_hwm_nodes() == true);
    assert(tree_with_all_pruning.is_pruning_parent_levels() == true);

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_materialized_real_node_count() {
    start_test(__func__);

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
    MaterializedBinaryTree<T> tree_with_no_pruning(levels, opts_with_no_pruning);
    assert(tree_with_no_pruning.real_node_count() == 255);
    MaterializedBinaryTree<T> tree_with_hwm_pruning(levels, opts_with_hwm_pruning);
    assert(tree_with_hwm_pruning.real_node_count() == 33);
    MaterializedBinaryTree<T> tree_with_parent_pruning(levels, opts_with_parent_pruning);
    assert(tree_with_parent_pruning.real_node_count() == 128);
    MaterializedBinaryTree<T> tree_with_all_pruning(levels, opts_with_all_pruning);
    assert(tree_with_all_pruning.real_node_count() == 13);

    // Compare real node count to level map count.
    MaterializedBinaryTree<T> tree(levels);
    T expected_count = (T{1} << levels) - 1;
    T map_count = 0;
    const auto& map = tree.get_level_map();
    for (level_t i = 1; i <= tree.get_level_count(); i++) {
        map_count += map.at(i).size();
    }
    assert(tree.real_node_count() == expected_count);
    assert(map_count == expected_count);
    //
    // No implicit tree test here because there's no map.

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_implicit_uncovered_positions() {
    start_test(__func__);

    // Implicit
    BinaryTree<T, BinaryTreeImplicitImpl<T>> implicit_tree(1);
    assert(implicit_tree.get_level_count() == 1);
    const auto& uncovered_positions = implicit_tree.get_uncovered_positions();
    //
    // Implicit trees do not have a level map.  Instead, we verify their uncovered positions to ensure the remaining align with
    // known subtrees that "pruning" wouldn't have caught in a Materialized tree.
    // Positions shift based on the root node being 0-based or 1-based, fyi...
    size_t root_value = BinaryTreeMath<T>::get_root_value();
    if (root_value == 0) {
        //
        // Level 1: root
        assert(implicit_tree.get_root_node()->get_value() == 0 + BinaryTreeMath<T>::get_root_value());
        assert(uncovered_positions.cardinality() == 1);
        assert(uncovered_positions.contains(1));
        //
        // Level 2: 2 children, and only one of them is newly covered.  It's the number 2, which is in position 2.
        // This leaves positions 1-1 uncovered.
        implicit_tree.add_level();
        assert(uncovered_positions.cardinality() == 1);
        assert(uncovered_positions.contains(1));
        //
        // Level 3: 4 children, and only one of them is newly covered.  It's the number 5, which is in position 2.
        // However, 5 is adjacent to the other interval, which will be merged.  Final covered intervals is: [2-4]
        // This leaves positions 1-1 uncovered.
        implicit_tree.add_level();
        assert(uncovered_positions.cardinality() == 1);
        assert(uncovered_positions.contains(1));
        //
        // Level 4: 8 children, and no new ones are covered.
        // This leaves positions 3-8 covered in a single interval.
        // This leaves positions 1-2 uncovered in a single interval.
        implicit_tree.add_level();
        assert(uncovered_positions.cardinality() == 2);
        assert(uncovered_positions.contains(1));
        assert(uncovered_positions.contains(2));
        //
        // Level 5: 16 children, one is newly covered.  It's the number 19, which is in position 3.
        // This leaves 2 covered intervals: [3-3, 5-16].
        // This leaves 2 uncovered intervals: [1-2, 4-4]
        implicit_tree.add_level();
        assert(uncovered_positions.cardinality() == 3);
        assert(uncovered_positions.contains(1));
        assert(uncovered_positions.contains(2));
        assert(uncovered_positions.contains(4));
    } else {
        //
        // Level 1: root
        assert(implicit_tree.get_root_node()->get_value() == 0 + BinaryTreeMath<T>::get_root_value());
        assert(uncovered_positions.cardinality() == 1);
        assert(uncovered_positions.contains(1));
        //
        // Level 2: 2 children, and only one of them is newly covered.  It's the number 2, which is in position 1.
        // This leaves 1 uncovered interval: [2-2]
        implicit_tree.add_level();
        assert(uncovered_positions.cardinality() == 1);
        assert(uncovered_positions.contains(2));
        //
        // Level 3: 4 children, and only one of them is newly covered.  It's the number 5, which is in position 3.
        // This leaves 1 covered interval after merging: [1-3]
        // This leaves 1 uncovered interval: [4-4]
        implicit_tree.add_level();
        assert(uncovered_positions.cardinality() == 1);
        assert(uncovered_positions.contains(4));
        //
        // Level 4: 8 children, none are newly covered.
        // This leaves 1 uncovered interval scaled up: [7-8]
        implicit_tree.add_level();
        assert(uncovered_positions.cardinality() == 2);
        assert(uncovered_positions.contains(7));
        assert(uncovered_positions.contains(8));
        //
        // Level 5: 16 children, one is newly covered.  It's the number 19, which is in position 13.
        // This leaves 1 covered interval after merging: [1-13]
        // This leaves 1 uncovered interval: [14-16]
        implicit_tree.add_level();
        assert(uncovered_positions.cardinality() == 3);
        assert(uncovered_positions.contains(14));
        assert(uncovered_positions.contains(15));
        assert(uncovered_positions.contains(16));
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_assert_level_will_fit() {
    start_test(__func__);

    // The limits of type T are already tested in the BinaryTreeMath tests, but they'll be checked again here.
    level_t max_level = BinaryTreeMath<T>::st_max_level_of_type();

    // Implicit
    // 8 bit can't build a 16 level tree
    try {
        ImplicitBinaryTree<uint8_t> tree_implicit(16);
        assert(false); // Should throw
    } catch (const std::out_of_range& e) {
        assert(std::string(e.what()).find("Cannot build a BinaryTree with 16") != std::string::npos);
    }
    // A 32-bit tree type can.
    ImplicitBinaryTree<uint32_t> tree_implicit(16);
    // An 8 bit can, however,  handle a 4-level tree.
    ImplicitBinaryTree<uint8_t> tree2_implicit(4);
    // It can make a 7-level tree too.
    ImplicitBinaryTree<uint8_t> tree3_implicit(7);
    // Adding another level should break.
    try {
        tree3_implicit.add_level();
        assert(false); // Should throw
    } catch (const std::out_of_range& e) {
        assert(std::string(e.what()).find("Cannot build a BinaryTree with 8") != std::string::npos);
    }
    // Build up to type T's max.
    if (FixedWidthIntegral<T>) {
        ImplicitBinaryTree<T> tree_implicit_empty;
        tree_implicit_empty.assert_level_will_fit(max_level);
        try {
            ImplicitBinaryTree<T> tree_implicit(max_level + 1);
            assert(false); // Should throw
        } catch (const std::out_of_range& e) {
            assert(std::string(e.what()).find("Cannot build a BinaryTree with " + std::to_string(max_level + 1)) != std::string::npos);
        }
    }

    // Materialized
    // 8 bit can't build a 16 level tree
    try {
        MaterializedBinaryTree<uint8_t> tree(16);
        assert(false); // Should throw
    } catch (const std::out_of_range& e) {
        assert(std::string(e.what()).find("Cannot build a BinaryTree with 16") != std::string::npos);
    }
    // A 32-bit tree type can.
    MaterializedBinaryTree<uint32_t> tree(16);
    // An 8 bit can, however,  handle a 4-level tree.
    MaterializedBinaryTree<uint8_t> tree2(4);
    // It can make a 7-level tree too.
    MaterializedBinaryTree<uint8_t> tree3(7);
    // Adding another level should break.
    try {
        tree3.add_level();
        assert(false); // Should throw
    } catch (const std::out_of_range& e) {
        assert(std::string(e.what()).find("Cannot build a BinaryTree with 8") != std::string::npos);
    }
    // Build up to type T's max.
    if (FixedWidthIntegral<T>) {
        MaterializedBinaryTree<T> tree_materialized_empty;
        tree_materialized_empty.assert_level_will_fit(max_level);
        try {
            MaterializedBinaryTree<T> tree_materialized(max_level + 1);
            assert(false); // Should throw
        } catch (const std::out_of_range& e) {
            assert(std::string(e.what()).find("Cannot build a BinaryTree with " + std::to_string(max_level + 1)) != std::string::npos);
        }
    }


    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_assert_level_verification() {
    start_test(__func__);

    // Implicit
    ImplicitBinaryTree<T> implicit_tree;
    // Testing to the known limit without verification is okay.
    for (level_t level = 1; level < CollatzConstants::LARGEST_EMPIRICALLY_TESTED_LEVEL; level++) {
        implicit_tree.assert_level_verification(level, false);
    }
    // Building over the max level should fail without verification.
    try {
        implicit_tree.assert_level_verification(CollatzConstants::LARGEST_EMPIRICALLY_TESTED_LEVEL + 1, false);
        assert(false); // Should throw
    } catch (const std::out_of_range& e) {
        assert(std::string(e.what()).find("Tree has reached max level of previously verified space") != std::string::npos);
    }
    // With verification enabled, should be okay.
    implicit_tree.assert_level_verification(CollatzConstants::LARGEST_EMPIRICALLY_TESTED_LEVEL + 1, true);

    // Implicit
    MaterializedBinaryTree<T> materialized_tree;
    // Testing to the known limit without verification is okay.
    for (level_t level = 1; level < CollatzConstants::LARGEST_EMPIRICALLY_TESTED_LEVEL; level++) {
        materialized_tree.assert_level_verification(level, false);
    }
    // Building over the max level should fail without verification.
    try {
        materialized_tree.assert_level_verification(CollatzConstants::LARGEST_EMPIRICALLY_TESTED_LEVEL + 1, false);
        assert(false); // Should throw
    } catch (const std::out_of_range& e) {
        assert(std::string(e.what()).find("Tree has reached max level of previously verified space") != std::string::npos);
    }
    // With verification enabled, should be okay.
    materialized_tree.assert_level_verification(CollatzConstants::LARGEST_EMPIRICALLY_TESTED_LEVEL + 1, true);

    end_test();
}



template<typename TreeType, AnySupportedIntegral T>
void confirm_coverage(BinaryTree<T, TreeType>& tree) {
    BinaryTreeCoverage<T> global_coverage;
    T level_target_total = 0;
    for (size_t level=1; level<=tree.get_level_count(); level++) {
        level_target_total = BinaryTreeMath<T>::st_node_count_of_level(level);
        const BinaryTreeCoverage<T>* level_coverage = &tree.get_coverage_map().find(level)->second;
        global_coverage.add_covered(level_coverage->get_covered());
        global_coverage.add_total(level_coverage->get_total());
        assert(level_coverage->get_covered() == BinaryTreeCoverageConstants::get_known_coverage<T>(level));
        assert(level_coverage->get_total() == level_target_total);
    }
    assert(global_coverage.get_covered() == BinaryTreeCoverageConstants::get_known_coverage_sum_to_level<T>(tree.get_level_count()));
    assert(global_coverage.get_total() == BinaryTreeCoverageConstants::get_total_sum_of_levels<T>(1, tree.get_level_count()));
}
template<AnySupportedIntegral T>
void test_binary_tree_add_level() {
    start_test(__func__);

    // Large-scale trees should work.
    // Doesn't work for materialized, because it'll eat all the RAM.
    // Use 33+ to activate multiple prefixes in NodeBitmap.
    ImplicitBinaryTree<T> big_tree(33);
    confirm_coverage(big_tree);

    // Multi-threading should produce identical results.
    // Pruning shouldn't affect coverage totals.
    BinaryTreeOptions opts_hwm_pruning_only;
    opts_hwm_pruning_only.prune_hwm_nodes = true;
    opts_hwm_pruning_only.prune_parent_levels = false;
    BinaryTreeOptions opts_all_pruning;
    opts_all_pruning.prune_hwm_nodes = true;
    opts_all_pruning.prune_parent_levels = true;
    BinaryTreeOptions opts_level_pruning_only;
    opts_level_pruning_only.prune_hwm_nodes = false;
    opts_level_pruning_only.prune_parent_levels = true;
    BinaryTreeOptions opts_no_pruning;
    opts_no_pruning.prune_hwm_nodes = false;
    opts_no_pruning.prune_parent_levels = false;
    int original_thread_count = omp_get_max_threads();
    level_t max_level = 16;
    for (level_t level = 1; level < max_level; level++) {
        for (int threads = 1; threads < 8; threads++) {
            omp_set_num_threads(threads);
            // Implicit (Pruning doesn't exist, but shouldn't cause problems when set)
            ImplicitBinaryTree<T> implicit_tree_1(level, opts_no_pruning);
            ImplicitBinaryTree<T> implicit_tree_2(level, opts_hwm_pruning_only);
            ImplicitBinaryTree<T> implicit_tree_3(level, opts_level_pruning_only);
            ImplicitBinaryTree<T> implicit_tree_4(level, opts_all_pruning);
            confirm_coverage(implicit_tree_1);
            confirm_coverage(implicit_tree_2);
            confirm_coverage(implicit_tree_3);
            confirm_coverage(implicit_tree_4);
            // Materialized
            MaterializedBinaryTree<T> materialized_tree_1(level, opts_no_pruning);
            MaterializedBinaryTree<T> materialized_tree_2(level, opts_hwm_pruning_only);
            MaterializedBinaryTree<T> materialized_tree_3(level, opts_level_pruning_only);
            MaterializedBinaryTree<T> materialized_tree_4(level, opts_all_pruning);
            confirm_coverage(materialized_tree_1);
            confirm_coverage(materialized_tree_2);
            confirm_coverage(materialized_tree_3);
            confirm_coverage(materialized_tree_4);
        }
    }
    omp_set_num_threads(original_thread_count);

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_deep_size() {
    start_test(__func__);

    MaterializedBinaryTree<T> tree(2);
    size_t size = tree.deep_size();
    assert(size > sizeof(tree)); // Make sure something was counted
    ImplicitBinaryTree<T> tree_implicit(2);
    size_t size_implicit = tree_implicit.deep_size();
    assert(size_implicit > sizeof(tree_implicit)); // Make sure something was counted

    end_test();
}



template<AnySupportedIntegral T, typename TreeType>
void sle_assert_trees_equal(const BinaryTree<T, TreeType>& first, const BinaryTree<T, TreeType>& second) {
    std::string err;
    // Test that first->first is true.
    if (! first.equal(first, &err)) {
        std::cout << "Tree equality to itself (first->first) failed using member helper.  Error chain is: " << err << std::endl;
        assert(false);
    }
    // Test first->second
    if (! first.equal(second, &err)) {
        std::cout << "Tree equality (first->second) failed using member helper.  Error chain is: " << err << std::endl;
        assert(false);
    }
    // Test bidirectionally second->first
    if (! second.equal(first, &err)) {
        std::cout << "Tree equality (second->first) failed using member helper.  Error chain is: " << err << std::endl;
        assert(false);
    }
    // Test static method direct invocation
    if (! BinaryTree<T, TreeType>::st_equal(first, second, &err)) {
        std::cout << "Tree equality failed using static method.  Error chain is: " << err << std::endl;
        assert(false);
    }
}
template<AnySupportedIntegral T, typename TreeType>
void sle_assert_trees_unequal(const BinaryTree<T, TreeType>& first, const BinaryTree<T, TreeType>& second, std::string& known_err) {
    std::string err;
    if (first.equal(second, &err)) {
        std::cout << "Tree equality succeeded when it should fail, using member helper.  Error chain is: " << err << std::endl;
        assert(false);
    }
    if (err.find(known_err) == std::string::npos) {
        std::cout << "Expected to find this in err: " << known_err << "  ---- Got this in err instead, which didn't match: " << err << std::endl;
        assert(false);
    }
    assert(err.find(known_err) != std::string::npos);
    if (BinaryTree<T, TreeType>::st_equal(first, second, &err)) {
        std::cout << "Tree equality succeeded when it should fail, using static method.  Error chain is: " << err << std::endl;
        assert(false);
    }
}
template<AnySupportedIntegral T, typename TreeType>
void sle_helper(
    size_t levels
    , size_t different_levels
    , BinaryTreeOptions opts
    , BinaryTreeOptions different_opts
    , std::string& known_err
) {
    namespace fs = std::filesystem;

    //
    // Build Trees
    //
    BinaryTree<T, TreeType> tree(levels, opts);
    BinaryTree<T, TreeType> tree_duplicate(levels, opts);
    BinaryTree<T, TreeType> tree_different(different_levels, different_opts);
    std::string err;

    //
    // In-Memory Equality
    //
    // Trees of the same construction should match.
    sle_assert_trees_equal(tree, tree_duplicate);
    // They should match bi-directionally too.
    sle_assert_trees_equal(tree, tree_duplicate);
    // Different trees should fail equality and emit a message.
    sle_assert_trees_unequal(tree, tree_different, known_err);
    //
    // Compute value maps.
    tree.generate_value_map();
    tree_duplicate.generate_value_map();
    tree_different.generate_value_map();
    //
    // They should still be equal/unequal.
    sle_assert_trees_equal(tree, tree_duplicate);
    sle_assert_trees_equal(tree, tree_duplicate);
    sle_assert_trees_unequal(tree, tree_different, known_err);

    //
    // Save and On-Disk Equality
    //
    // Use common file paths for all tests.
    const std::string tree_path = "__tree.btree";
    const std::string tree_duplicate_path = "__tree_duplicate.btree";
    const std::string tree_different_path = "__tree_different.btree";
    const std::string ctree_path = "__tree.btree.zst";
    const std::string ctree_duplicate_path = "__tree_duplicate.btree.zst";
    const std::string ctree_different_path = "__tree_different.btree.zst";
    // Delete existing files if they weren't cleaned up properly last time.
    if (fs::exists(tree_path)) { fs::remove(tree_path); }
    if (fs::exists(tree_duplicate_path)) { fs::remove(tree_duplicate_path); }
    if (fs::exists(tree_different_path)) { fs::remove(tree_different_path); }
    if (fs::exists(ctree_path)) { fs::remove(ctree_path); }
    if (fs::exists(ctree_duplicate_path)) { fs::remove(ctree_duplicate_path); }
    if (fs::exists(ctree_different_path)) { fs::remove(ctree_different_path); }
    // Save function should return true/successful.
    assert(tree.save(tree_path, 0));
    assert(tree_duplicate.save(tree_duplicate_path, 0));
    assert(tree_different.save(tree_different_path, 0));
    assert(tree.save(ctree_path, 22));
    assert(tree_duplicate.save(ctree_duplicate_path, 22));
    assert(tree_different.save(ctree_different_path, 22));
    // Compressed versions should be smaller than raw versions.
    assert(fs::file_size(tree_path) > fs::file_size(ctree_path));
    assert(fs::file_size(tree_duplicate_path) > fs::file_size(ctree_duplicate_path));
    assert(fs::file_size(tree_different_path) > fs::file_size(ctree_different_path));
    // Open file handles to each file.
    std::ifstream fh_tree(tree_path, std::ios::binary);
    std::ifstream fh_tree_duplicate(tree_duplicate_path, std::ios::binary);
    std::ifstream fh_tree_different(tree_different_path, std::ios::binary);
    std::ifstream fh_ctree(ctree_path, std::ios::binary);
    std::ifstream fh_ctree_duplicate(ctree_duplicate_path, std::ios::binary);
    std::ifstream fh_ctree_different(ctree_different_path, std::ios::binary);
    // Ensure they're actually open.
    assert(fh_tree.is_open());
    assert(fh_tree_duplicate.is_open());
    assert(fh_tree_different.is_open());
    assert(fh_ctree.is_open());
    assert(fh_ctree_duplicate.is_open());
    assert(fh_ctree_different.is_open());
    // The two equal trees should create equal files on-disk.  The different one should be different, of course.
    assert(std::equal(std::istreambuf_iterator<char>(fh_tree), std::istreambuf_iterator<char>(), std::istreambuf_iterator<char>(fh_tree_duplicate)));
    assert(std::equal(std::istreambuf_iterator<char>(fh_ctree), std::istreambuf_iterator<char>(), std::istreambuf_iterator<char>(fh_ctree_duplicate)));
    fh_tree.clear();
    fh_ctree.clear();
    fh_tree.seekg(0, std::ios::beg);
    fh_ctree.seekg(0, std::ios::beg);
    assert(! std::equal(std::istreambuf_iterator<char>(fh_tree), std::istreambuf_iterator<char>(), std::istreambuf_iterator<char>(fh_tree_different)));
    assert(! std::equal(std::istreambuf_iterator<char>(fh_ctree), std::istreambuf_iterator<char>(), std::istreambuf_iterator<char>(fh_ctree_different)));
    // Close the file handles.
    fh_tree.close();
    fh_tree_duplicate.close();
    fh_tree_different.close();
    fh_ctree.close();
    fh_ctree_duplicate.close();
    fh_ctree_different.close();

    //
    // Load and Test In-Memory Equality Again
    //
    // Make empty trees.
    BinaryTree<T, TreeType> l_tree;
    BinaryTree<T, TreeType> l_tree_duplicate;
    BinaryTree<T, TreeType> l_tree_different;
    BinaryTree<T, TreeType> l_ctree;
    BinaryTree<T, TreeType> l_ctree_duplicate;
    BinaryTree<T, TreeType> l_ctree_different;
    // Load the trees from file data.
    assert(l_tree.load(tree_path));
    assert(l_tree_duplicate.load(tree_duplicate_path));
    assert(l_tree_different.load(tree_different_path));
    assert(l_ctree.load(ctree_path));
    assert(l_ctree_duplicate.load(ctree_duplicate_path));
    assert(l_ctree_different.load(ctree_different_path));
    // Loaded trees should match the in-memory versions before we saved.
    sle_assert_trees_equal(l_tree, tree);
    sle_assert_trees_equal(l_tree_duplicate, tree_duplicate);
    sle_assert_trees_equal(l_tree_different, tree_different);
    sle_assert_trees_equal(l_ctree, tree);
    sle_assert_trees_equal(l_ctree_duplicate, tree_duplicate);
    sle_assert_trees_equal(l_ctree_different, tree_different);
    // Ensure the Uncovered Value Bitmaps Survived
    assert(l_tree.get_uncovered_values().empty() == false);
    assert(l_tree_duplicate.get_uncovered_values().empty() == false);
    assert(l_tree_different.get_uncovered_values().empty() == false);

    //
    // Clean-up
    //
    // Delete the files.
    if (fs::exists(tree_path)) { fs::remove(tree_path); }
    if (fs::exists(tree_duplicate_path)) { fs::remove(tree_duplicate_path); }
    if (fs::exists(tree_different_path)) { fs::remove(tree_different_path); }
    if (fs::exists(ctree_path)) { fs::remove(ctree_path); }
    if (fs::exists(ctree_duplicate_path)) { fs::remove(ctree_duplicate_path); }
    if (fs::exists(ctree_different_path)) { fs::remove(ctree_different_path); }
}
template<AnySupportedIntegral T>
void test_binary_tree_save_load_equal() {
    start_test(__func__);

    namespace fs = std::filesystem;

    // Saving to a console via stdout ("-") should fail.
    try {
        ImplicitBinaryTree<T> tree_bad_stdout_save(8);
        tree_bad_stdout_save.save("-");
        assert(false);
    } catch(const std::runtime_error& e) {
        assert(std::string(e.what()).find("Refusing to write binary data to a terminal") != std::string::npos);
    }

    // Start with smaller trees
    const size_t INITIAL_LEVELS = 15;
    size_t levels = INITIAL_LEVELS;

    // Level count differs.
    size_t different_levels = levels + 1;
    BinaryTreeOptions opts;
    BinaryTreeOptions different_opts;
    std::string known_err = "Level counts differ";
    sle_helper<T, BinaryTreeImplicitImpl<T>>(levels, different_levels, opts, different_opts, known_err);
    sle_helper<T, BinaryTreeMaterializedImpl<T>>(levels, different_levels, opts, different_opts, known_err);

    // Preserve ancestors differs.
    different_levels = levels;
    opts.reset();
    different_opts.reset();
    different_opts.preserve_ancestors = true;
    known_err = "Is preserving ancestors mismatch";
    sle_helper<T, BinaryTreeImplicitImpl<T>>(levels, different_levels, opts, different_opts, known_err);
    sle_helper<T, BinaryTreeMaterializedImpl<T>>(levels, different_levels, opts, different_opts, known_err);

    // Verifying HWM nodes differs.
    different_levels = levels;
    opts.reset();
    different_opts.reset();
    different_opts.verify_non_hwm_nodes = true;
    known_err = "Is verifying hwm nodes mismatch";
    sle_helper<T, BinaryTreeImplicitImpl<T>>(levels, different_levels, opts, different_opts, known_err);
    sle_helper<T, BinaryTreeMaterializedImpl<T>>(levels, different_levels, opts, different_opts, known_err);

    // Pruning node should always throw a logic error, because deserializing it is a nightmare I'm not interested in.
    opts.reset();
    opts.prune_hwm_nodes = true;
    const std::string tree_path = "__tree.btree.zst";
    if (fs::exists(tree_path)) { fs::remove(tree_path); }
    try {
        MaterializedBinaryTree<T> tree(levels, opts);
        tree.save(tree_path);
        assert(false);
    } catch(const std::logic_error& e) {
        assert(std::string(e.what()).find("You've activated my trap card!") != std::string::npos);
    }
    if (fs::exists(tree_path)) { fs::remove(tree_path); }
    opts.reset();
    opts.prune_parent_levels = true;
    try {
        MaterializedBinaryTree<T> tree(levels, opts);
        tree.save(tree_path);
        assert(false);
    } catch(const std::logic_error& e) {
        assert(std::string(e.what()).find("You've activated my trap card!") != std::string::npos);
    }
    if (fs::exists(tree_path)) { fs::remove(tree_path); }

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_st_generate_node_at() {
    start_test(__func__);

    // Materialized Tree feature, but owned by the facade.
    size_t root_value = BinaryTreeMath<T>::get_root_value();
    Node<T>* node = nullptr;
    node = BinaryTree<T>::st_generate_node_at(2, 1);
    assert(node != nullptr);
    assert(node->get_value() == 1 + root_value);
    delete node;
    node = BinaryTree<T>::st_generate_node_at(3, 1);
    assert(node != nullptr);
    assert(node->get_value() == 3 + root_value);
    delete node;
    node = BinaryTree<T>::st_generate_node_at(3, 4);
    assert(node != nullptr);
    assert(node->get_value() == 6 + root_value);
    delete node;
    node = BinaryTree<T>::st_generate_node_at(4, 4);
    assert(node != nullptr);
    assert(node->get_value() == 13 + root_value);
    delete node;
    node = BinaryTree<T>::st_generate_node_at(4, 5);
    assert(node != nullptr);
    assert(node->get_value() == 8 + root_value);
    delete node;
    node = BinaryTree<T>::st_generate_node_at(4, 8);
    assert(node != nullptr);
    assert(node->get_value() == 14 + root_value);
    delete node;
    node = BinaryTree<T>::st_generate_node_at(5, 12);
    assert(node != nullptr);
    assert(node->get_value() == 28 + root_value);
    delete node;

    // Invalid positions should throw.
    try {
        BinaryTree<T>::st_generate_node_at(3, 0);
        assert(false); // Should throw
    } catch (const std::out_of_range& e) {
        assert(std::string(e.what()).find("position 0") != std::string::npos);
    }
    //
    try {
        BinaryTree<T>::st_generate_node_at(3, 9); // 2^3 = 8 max
        assert(false); // Should throw
    } catch (const std::out_of_range& e) {
        assert(std::string(e.what()).find("outside of a level") != std::string::npos);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_generate_value_map() {
    start_test(__func__);

    level_t levels = 8;

    // Implicit
    BinaryTree<T, BinaryTreeImplicitImpl<T>> implicit_tree(levels);
    assert(implicit_tree.get_uncovered_values().empty() == true);
    implicit_tree.generate_value_map();
    assert(implicit_tree.get_uncovered_values().empty() == false);
    assert(implicit_tree.get_uncovered_values().cardinality() == implicit_tree.get_uncovered_positions().cardinality());
    // Adding a level should clear the map.
    implicit_tree.add_level();
    assert(implicit_tree.get_uncovered_values().empty() == true);
    // Generating it again shouldn't be an issue.
    implicit_tree.generate_value_map();
    assert(implicit_tree.get_uncovered_values().empty() == false);
    assert(implicit_tree.get_uncovered_values().cardinality() == implicit_tree.get_uncovered_positions().cardinality());

    // Materialized
    BinaryTree<T, BinaryTreeMaterializedImpl<T>> materialized_tree(levels);
    assert(materialized_tree.get_uncovered_values().empty() == true);
    materialized_tree.generate_value_map();
    assert(materialized_tree.get_uncovered_values().empty() == false);
    T uncovered_nodes_in_map = 0;
    for (const Node<T>* node : materialized_tree.get_level_map().at(levels)) {
        if (node->is_below_high_water_mark() || node->has_high_water_mark_ancestor()) {
            continue;
        }
        uncovered_nodes_in_map++;
    }
    assert(materialized_tree.get_uncovered_values().cardinality() == uncovered_nodes_in_map);
    // Adding a level should clear the map.
    materialized_tree.add_level();
    assert(materialized_tree.get_uncovered_values().empty() == true);
    // Generating it again shouldn't be an issue.
    materialized_tree.generate_value_map();
    assert(materialized_tree.get_uncovered_values().empty() == false);
    uncovered_nodes_in_map = 0;
    for (const Node<T>* node : materialized_tree.get_level_map().at(levels+1)) {
        if (node->is_below_high_water_mark() || node->has_high_water_mark_ancestor()) {
            continue;
        }
        uncovered_nodes_in_map++;
    }
    assert(materialized_tree.get_uncovered_values().cardinality() == uncovered_nodes_in_map);

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_clear_uncovered_values() {
    start_test(__func__);

    // Implicit
    ImplicitBinaryTree<T> implicit_tree(8);
    assert(implicit_tree.get_uncovered_values().empty() == true);
    implicit_tree.generate_value_map();
    assert(implicit_tree.get_uncovered_values().empty() == false);
    implicit_tree.clear_uncovered_values();
    assert(implicit_tree.get_uncovered_values().empty() == true);

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_for_each_uncovered_value() {
    start_test(__func__);

    level_t levels = 8;
    size_t count = 0;
    std::atomic<size_t> a_count = 0;
    T previous_value = 0;

    // Implicit
    BinaryTree<T, BinaryTreeImplicitImpl<T>> implicit_tree(levels);
    assert(implicit_tree.get_uncovered_values().empty() == true);
    // -- Failure to generate the map ahead of time should throw an error.
    try {
        implicit_tree.for_each_uncovered_value(ForEachPolicy::SERIAL, [&](const T& value) {
            if (value % 128 == 0) {
                count++;
            }
            if (value > (T(1) << (levels + 3))) {
                return ForEachSignal::BREAK;
            }
            return ForEachSignal::CONTINUE;
        });
        assert(false);
    } catch (std::runtime_error& e) {
        assert(std::string(e.what()).find("Value map not generated.") != std::string::npos);
    }
    implicit_tree.generate_value_map();
    assert(implicit_tree.get_uncovered_values().empty() == false);
    // -- Serial
    count = 0;
    implicit_tree.for_each_uncovered_value(ForEachPolicy::SERIAL, [&](const T& value) {
        if (value % 128 == 0) {
            count++;
        }
        assert(previous_value < value);
        previous_value = value;
        if (value > (T(1) << (levels + 3))) {
            return ForEachSignal::BREAK;
        }
        return ForEachSignal::CONTINUE;
    });
    assert(count == 0);
    // -- Parallel
    a_count.store(0);
    previous_value = 0;
    implicit_tree.for_each_uncovered_value(ForEachPolicy::PARALLEL, [&](const T& value) {
        if (value % 128 == 0) {
            a_count.fetch_add(1);
        }
        assert(previous_value < value);
        previous_value = value;
        if (value > (T(1) << (levels + 3))) {
            return ForEachSignal::BREAK;
        }
        return ForEachSignal::CONTINUE;
    });
    assert(a_count.load() == 0);

    // Materialized
    BinaryTree<T, BinaryTreeMaterializedImpl<T>> materialized_tree(levels);
    assert(materialized_tree.get_uncovered_values().empty() == true);
    // -- Failure to generate the map ahead of time should throw an error.
    try {
        materialized_tree.for_each_uncovered_value(ForEachPolicy::SERIAL, [&](const T& value) {
            if (value % 128 == 0) {
                count++;
            }
            if (value > (T(1) << (levels + 3))) {
                return ForEachSignal::BREAK;
            }
            return ForEachSignal::CONTINUE;
        });
        assert(false);
    } catch (std::runtime_error& e) {
        assert(std::string(e.what()).find("Value map not generated.") != std::string::npos);
    }
    materialized_tree.generate_value_map();
    assert(materialized_tree.get_uncovered_values().empty() == false);
    // -- Serial
    count = 0;
    materialized_tree.for_each_uncovered_value(ForEachPolicy::SERIAL, [&](const T& value) {
        if (value % 128 == 0) {
            count++;
        }
        if (value > (T(1) << (levels + 3))) {
            return ForEachSignal::BREAK;
        }
        return ForEachSignal::CONTINUE;
    });
    assert(count == 0);
    // -- Parallel
    a_count.store(0);
    materialized_tree.for_each_uncovered_value(ForEachPolicy::PARALLEL, [&](const T& value) {
        if (value % 128 == 0) {
            a_count.fetch_add(1);
        }
        if (value > (T(1) << (levels + 3))) {
            return ForEachSignal::BREAK;
        }
        return ForEachSignal::CONTINUE;
    });
    assert(a_count.load() == 0);

    end_test();
}



template<AnySupportedIntegral T, typename TreeType>
void for_each_manual_test_helper(level_t level, std::vector<T> known_survivors) {
    // Settings
    const T manual_test_value = 10000;
    size_t known_survivor_index = 99999;
    T scaling_factor = T(1) << (level - 1);
    BinaryTree<T, TreeType> tree(level);

    // Run it
    tree.generate_value_map();
    tree.for_each_uncovered_value(ForEachPolicy::SERIAL, [&](const T& value) {
        if (known_survivor_index >= known_survivors.size()) {
            known_survivor_index = 0;
            for (T& known_survivor : known_survivors) {
                known_survivor += scaling_factor;
            }
        }
        assert(value == known_survivors.at(known_survivor_index));
        if (known_survivors.at(known_survivor_index) > manual_test_value) {
            return ForEachSignal::BREAK;
        }
        known_survivor_index++;
        return ForEachSignal::CONTINUE;
    });
}
template<AnySupportedIntegral T>
void test_binary_tree_for_each_uncovered_value_with_tls() {
    start_test(__func__);

    level_t levels = 8;
    std::vector<size_t> count_vec(0);
    T previous_value = 0;

    // Implicit
    BinaryTree<T, BinaryTreeImplicitImpl<T>> implicit_tree(levels);
    assert(implicit_tree.get_uncovered_values().empty() == true);
    // -- Failure to generate the map ahead of time should throw an error.
    try {
        count_vec.clear();
        implicit_tree.for_each_uncovered_value_with_tls(ForEachPolicy::SERIAL, count_vec, [&](const T& value, size_t& my_count) {
            if (value % 128 == 0) {
                my_count++;
            }
            if (value > (T(1) << (levels + 1))) {
                return ForEachSignal::BREAK;
            }
            return ForEachSignal::CONTINUE;
        });
        assert(false);
    } catch (std::runtime_error& e) {
        assert(std::string(e.what()).find("Value map not generated.") != std::string::npos);
    }
    implicit_tree.generate_value_map();
    assert(implicit_tree.get_uncovered_values().empty() == false);
    // -- Serial
    count_vec.clear();
    implicit_tree.for_each_uncovered_value_with_tls(ForEachPolicy::SERIAL, count_vec, [&](const T& value, size_t& my_count) {
        if (value % 128 == 0) {
            my_count++;
        }
        assert(previous_value < value);
        previous_value = value;
        if (value > (T(1) << (levels + 1))) {
            return ForEachSignal::BREAK;
        }
        return ForEachSignal::CONTINUE;
    });
    for (const size_t& count : count_vec) {
        assert(count == 0);
    }
    // -- Parallel
    count_vec.clear();
    implicit_tree.for_each_uncovered_value_with_tls(ForEachPolicy::PARALLEL, count_vec, [&](const T& value, size_t& my_count) {
        if (value % 128 == 0) {
            my_count++;
        }
        if (value > (T(1) << (levels + 1))) {
            return ForEachSignal::BREAK;
        }
        return ForEachSignal::CONTINUE;
    });
    for (const size_t& count : count_vec) {
        assert(count == 0);
    }

    // Materialized
    BinaryTree<T, BinaryTreeMaterializedImpl<T>> materialized_tree(levels);
    assert(materialized_tree.get_uncovered_values().empty() == true);
    // -- Failure to generate the map ahead of time should throw an error.
    try {
        count_vec.clear();
        materialized_tree.for_each_uncovered_value_with_tls(ForEachPolicy::SERIAL, count_vec, [&](const T& value, size_t& my_count) {
            if (value % 128 == 0) {
                my_count++;
            }
            if (value > (T(1) << (levels + 1))) {
                return ForEachSignal::BREAK;
            }
            return ForEachSignal::CONTINUE;
        });
        assert(false);
    } catch (std::runtime_error& e) {
        assert(std::string(e.what()).find("Value map not generated.") != std::string::npos);
    }
    materialized_tree.generate_value_map();
    assert(materialized_tree.get_uncovered_values().empty() == false);
    // -- Serial
    count_vec.clear();
    previous_value = 0;
    materialized_tree.for_each_uncovered_value_with_tls(ForEachPolicy::SERIAL, count_vec, [&](const T& value, size_t& my_count) {
        if (value % 128 == 0) {
            my_count++;
        }
        assert(previous_value < value);
        previous_value = value;
        if (value > (T(1) << (levels + 1))) {
            return ForEachSignal::BREAK;
        }
        return ForEachSignal::CONTINUE;
    });
    for (const size_t& count : count_vec) {
        assert(count == 0);
    }
    // -- Parallel
    count_vec.clear();
    materialized_tree.for_each_uncovered_value_with_tls(ForEachPolicy::PARALLEL, count_vec, [&](const T& value, size_t& my_count) {
        if (value % 128 == 0) {
            my_count++;
        }
        if (value > (T(1) << (levels + 1))) {
            return ForEachSignal::BREAK;
        }
        return ForEachSignal::CONTINUE;
    });
    for (const size_t& count : count_vec) {
        assert(count == 0);
    }

    // -- Manually Test Remaining Values
    std::vector<T> known_survivors;
    // Level 2
    if (BinaryTreeMath<T>::get_root_value() == 0) {
        known_survivors.push_back(1);
    } else {
        known_survivors.push_back(3);
    }
    for_each_manual_test_helper<T, BinaryTreeImplicitImpl<T>>(2, known_survivors);
    for_each_manual_test_helper<T, BinaryTreeMaterializedImpl<T>>(2, known_survivors);
    // Level 3
    known_survivors.clear();
    if (BinaryTreeMath<T>::get_root_value() == 0) {
        known_survivors.push_back(3);
    } else {
        known_survivors.push_back(7);
    }
    for_each_manual_test_helper<T, BinaryTreeImplicitImpl<T>>(3, known_survivors);
    for_each_manual_test_helper<T, BinaryTreeMaterializedImpl<T>>(3, known_survivors);
    // Level 4
    known_survivors.clear();
    if (BinaryTreeMath<T>::get_root_value() == 0) {
        known_survivors.push_back(7);
        known_survivors.push_back(11);
    } else {
        known_survivors.push_back(11);
        known_survivors.push_back(15);
    }
    for_each_manual_test_helper<T, BinaryTreeImplicitImpl<T>>(4, known_survivors);
    for_each_manual_test_helper<T, BinaryTreeMaterializedImpl<T>>(4, known_survivors);
    // Level 5
    known_survivors.clear();
    if (BinaryTreeMath<T>::get_root_value() == 0) {
        known_survivors.push_back(15);
        known_survivors.push_back(23);
        known_survivors.push_back(27);
    } else {
        known_survivors.push_back(23);
        known_survivors.push_back(27);
        known_survivors.push_back(31);
    }
    for_each_manual_test_helper<T, BinaryTreeImplicitImpl<T>>(5, known_survivors);
    for_each_manual_test_helper<T, BinaryTreeMaterializedImpl<T>>(5, known_survivors);
    // Level 6
    known_survivors.clear();
    if (BinaryTreeMath<T>::get_root_value() == 0) {
        known_survivors.push_back(31);
        known_survivors.push_back(39);
        known_survivors.push_back(47);
        known_survivors.push_back(59);
    } else {
        known_survivors.push_back(39);
        known_survivors.push_back(47);
        known_survivors.push_back(59);
        known_survivors.push_back(63);
    }
    for_each_manual_test_helper<T, BinaryTreeImplicitImpl<T>>(6, known_survivors);
    for_each_manual_test_helper<T, BinaryTreeMaterializedImpl<T>>(6, known_survivors);

    // User-defined starting values should bump the multiplier to avoid rework.
    T start = 1000;  // Larger than 2^8 (level 8) by far.  Definitely need a bump to multiplier.
    T max_value = 100000;
    for(level_t level = 2; level <= 8; level++) {
        T scaling_factor = BinaryTreeMath<T>::st_scaling_factor(level + 1);

        // Implicit
        implicit_tree.init(level);
        implicit_tree.generate_value_map();
        implicit_tree.for_each_uncovered_value(ForEachPolicy::SERIAL, [&](const T& value) {
            assert(value > (start - scaling_factor));
            if (value > max_value) {
                return ForEachSignal::BREAK;
            }
            return ForEachSignal::CONTINUE;
        }, start);

        // Materialized
        materialized_tree.init(level);
        materialized_tree.generate_value_map();
        materialized_tree.for_each_uncovered_value(ForEachPolicy::SERIAL, [&](const T& value) {
            assert(value > (start - scaling_factor));
            if (value > max_value) {
                return ForEachSignal::BREAK;
            }
            return ForEachSignal::CONTINUE;
        }, start);
    }

    end_test();
}



template<AnySupportedIntegral T>
void run_all(size_t root_value) {
    std::string extra = "  Using " + std::to_string(root_value) + "-based tree root.";
    announce_run_all<T>(extra);

    BinaryTreeMath<T>::set_root_value(root_value);
    test_binary_tree_basic_construction<T>();
    test_binary_tree_init<T>();
    test_binary_tree_reset<T>();
    test_binary_tree_get_impl<T>();
    test_binary_tree_get_tree_type<T>();
    test_binary_tree_get_level_count<T>();
    test_binary_tree_get_scaling_factor<T>();
    test_binary_tree_get_root_node<T>();
    test_binary_tree_get_coverage_map<T>();
    test_binary_tree_get_ancestors<T>();
    test_binary_tree_is_verifying_non_hwm_nodes<T>();
    test_binary_tree_node_count<T>();
    test_binary_tree_materialized_level_map<T>();
    test_binary_tree_materialized_pruning_options<T>();
    test_binary_tree_materialized_real_node_count<T>();
    test_binary_tree_implicit_uncovered_positions<T>();
    test_binary_tree_assert_level_will_fit<T>();
    test_binary_tree_assert_level_verification<T>();
    test_binary_tree_add_level<T>();
    test_binary_tree_deep_size<T>();
    test_binary_tree_save_load_equal<T>();
    test_binary_tree_st_generate_node_at<T>();
    test_binary_tree_generate_value_map<T>();
    test_binary_tree_clear_uncovered_values<T>();
    test_binary_tree_for_each_uncovered_value<T>();
    test_binary_tree_for_each_uncovered_value_with_tls<T>();

    BinaryTreeMath<T>::reset_root_value();
}



int main() {
    std::string name = "BinaryTree";
    preamble(name);
    run_all<uint64_t>(0);
    run_all<uint64_t>(1);
    run_all<uint128_t>(0);
    run_all<uint128_t>(1);
    run_all<mpz_class>(0);
    run_all<mpz_class>(1);
    done(name);

    return 0;
}
