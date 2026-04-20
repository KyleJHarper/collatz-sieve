#include <cassert>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <gmpxx.h>
#include <stdexcept>
#include "../collatz/binary_tree.hpp"


//
// All of these tests need to work with native integrals and GMP.
// All of these tests need to work with 0-rooted and 1-rooted (traditional) binary trees.
// All of these tests need to work with Materialized and Implicit trees, where applicable.
//
// WHEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE
//


template<AnySupportedIntegral T>
void test_binary_tree_basic_construction() {
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
    assert(tree.get_root_node()->get_value() == BinaryTreeMath<T>::get_root_value());
    assert(tree.get_level_count() == 5);
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
    //
    //
    // Build an Implicit tree using the more complext signature.
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

    // Aliases should work.
    MaterializedBinaryTree<T> x(1);
    assert(x.is_materialized());
    ImplicitBinaryTree<T> y(1);
    assert(y.is_implicit());
}


template<AnySupportedIntegral T>
void test_binary_tree_deep_size() {
    MaterializedBinaryTree<T> tree(2);
    size_t size = tree.deep_size();
    assert(size > sizeof(tree)); // Make sure something was counted
    ImplicitBinaryTree<T> tree_implicit(2);
    size_t size_implicit = tree_implicit.deep_size();
    assert(size_implicit > sizeof(tree_implicit)); // Make sure something was counted
}


template<AnySupportedIntegral T>
void test_binary_tree_generate_node_at_valid() {
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
}


template<AnySupportedIntegral T>
void test_binary_tree_generate_node_at_invalid_pos() {
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
}


template<AnySupportedIntegral T, typename TreeType>
void test_binary_tree_coverage(size_t levels = 16, size_t threads = 1, const BinaryTreeOptions& opts = BinaryTree<T>::DEFAULT_OPTS) {
    // Set threads.
    omp_set_num_threads(threads);
    //
    // Build trees.
    BinaryTree<T, TreeType> tree(levels, opts);
    BinaryTreeCoverage<T> global_coverage;
    T level_target_total = 0;
    for (size_t level=1; level<=tree.get_level_count(); level++) {
        level_target_total = BinaryTreeMath<T>::st_node_count_of_level(level);
        const BinaryTreeCoverage<T>* level_coverage = &tree.get_coverage_map().find(level)->second;
        global_coverage.add_covered(level_coverage->get_covered());
        global_coverage.add_total(level_coverage->get_total());
        // std::cout << "\nI see level_coverage->get_covered()=" << to_string_any(level_coverage->get_covered())
        // << " and known_coverage=" << to_string_any(BinaryTreeCoverageConstants::get_known_coverage<T>(level))
        // << " on level=" << level << std::endl;
        assert(level_coverage->get_covered() == BinaryTreeCoverageConstants::get_known_coverage<T>(level));
        assert(level_coverage->get_total() == level_target_total);
    }
    assert(global_coverage.get_covered() == BinaryTreeCoverageConstants::get_known_coverage_sum_to_level<T>(levels));
    assert(global_coverage.get_total() == BinaryTreeCoverageConstants::get_total_sum_of_levels<T>(1, levels));
    //
    // Materialized trees have pruning options.
    if constexpr (std::same_as<TreeType, BinaryTreeMaterializedImpl<T>>) {
        // Tree should always have: 2^(level+1) - 2 nodes total if it's not pruned.
        // We allow pruning of HWM nodes and/or levels.  Need tests for each.
        //
        // Option 1: false and false
        // This is a tree with all its nodes.  Counts should line up without adjustment.
        if (!tree.is_pruning_hwm_nodes() && !tree.is_pruning_parent_levels()) {
            assert(tree.real_node_count() == (T{1} << (tree.get_level_count())) - 1);
        }
        // Option 2: true and false
        // HWM pruning is on, but levels are not erased.  Keeps a fulled pared list of non-HWM nodes.  Just add covered back in.
        if (tree.is_pruning_hwm_nodes() && !tree.is_pruning_parent_levels()) {
            T node_count_summary = tree.real_node_count();
            for (size_t level = 1; level <= tree.get_level_count(); level++) {
                node_count_summary += tree.get_coverage_map().find(level)->second.get_covered();
            }
            assert(node_count_summary == BinaryTreeCoverageConstants::get_total_sum_of_levels<T>(1, levels));
        }
        // Option 3: false and true
        // HWM pruning is off, but parent levels are removed.  Keeps a full final level and nothing more.  Add previous known coverage.
        if (!tree.is_pruning_hwm_nodes() && tree.is_pruning_parent_levels()) {
            T node_count_summary = tree.real_node_count() + BinaryTreeCoverageConstants::get_total_sum_of_levels<T>(1, levels - 1);
            assert(node_count_summary == BinaryTreeCoverageConstants::get_total_sum_of_levels<T>(1, levels));
        }
        // Option 4: true and true
        // HWM pruning is on, and parent levels are removed.  Keeps a pared-down final level of non-HWM nodes.
        if (tree.is_pruning_hwm_nodes() && tree.is_pruning_parent_levels()) {
            T node_count_plus_uncovered = tree.real_node_count();
            for (size_t level = 1; level < tree.get_level_count(); level++) {
                node_count_plus_uncovered += tree.get_coverage_map().find(level)->second.get_uncovered();
            }
            assert(node_count_plus_uncovered + BinaryTreeCoverageConstants::get_known_coverage_sum_to_level<T>(levels) == BinaryTreeCoverageConstants::get_total_sum_of_levels<T>(1, levels));
        }
    }
}


template<AnySupportedIntegral T>
void test_binary_tree_coverage_coherency_at_scale() {
    test_binary_tree_coverage<T, BinaryTreeImplicitImpl<T>>(32, 8);
}


template<AnySupportedIntegral T>
void test_binary_tree_node_count_should_match_map() {
    size_t levels = 5;
    MaterializedBinaryTree<T> tree(levels);
    T expected_count = (T{1} << levels) - 1;
    size_t map_count = 0;
    const auto& map = tree.get_level_map();
    for (size_t i = 1; i <= tree.get_level_count(); i++) {
        map_count += map.at(i).size();
    }
    assert(tree.real_node_count() == expected_count);
    assert(map_count == expected_count);
    //
    // No implicit tree test here because there's no map.
}


template<AnySupportedIntegral T>
void test_binary_tree_too_many_levels() {
    //
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
    //
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
}


template<AnySupportedIntegral T>
void test_binary_tree_multi_threaded() {
    BinaryTreeOptions opts = BinaryTree<T>::DEFAULT_OPTS;
    size_t max_threads = 4;
    // Materialized
    for (size_t threads = 1; threads <= max_threads; threads++) {
        test_binary_tree_coverage<T, BinaryTreeMaterializedImpl<T>>(16, threads, opts);
    }
    // Implicit
    for (size_t threads = 1; threads <= max_threads; threads++) {
        test_binary_tree_coverage<T, BinaryTreeImplicitImpl<T>>(16, threads, opts);
    }
}


template<AnySupportedIntegral T>
void test_binary_tree_math() {
    // Root Default
    assert(BinaryTreeMath<T>::get_default_root_value() == 1);
    //
    // Bit Reversal
    T value = 421;  // Binary: 110100101
    assert(BinaryTreeMath<T>::st_reverse_low_bits(value, 1) == 1);   //         1
    assert(BinaryTreeMath<T>::st_reverse_low_bits(value, 2) == 2);   //        10
    assert(BinaryTreeMath<T>::st_reverse_low_bits(value, 3) == 5);   //       101
    assert(BinaryTreeMath<T>::st_reverse_low_bits(value, 4) == 10);  //      1010
    assert(BinaryTreeMath<T>::st_reverse_low_bits(value, 5) == 20);  //     10100
    assert(BinaryTreeMath<T>::st_reverse_low_bits(value, 6) == 41);  //    101001
    assert(BinaryTreeMath<T>::st_reverse_low_bits(value, 7) == 82);  //   1010010
    assert(BinaryTreeMath<T>::st_reverse_low_bits(value, 8) == 165); //  10100101
    assert(BinaryTreeMath<T>::st_reverse_low_bits(value, 9) == 331); // 101001011
    //
    // Node Levels
    size_t level = 0;  // Always returns size_t
    level = BinaryTreeMath<T>::st_node_level(1);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 2) : assert(level == 1);
    level = BinaryTreeMath<T>::st_node_level(2);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 2) : assert(level == 2);
    level = BinaryTreeMath<T>::st_node_level(3);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 3) : assert(level == 2);
    level = BinaryTreeMath<T>::st_node_level(4);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 3) : assert(level == 3);
    level = BinaryTreeMath<T>::st_node_level(7);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 4) : assert(level == 3);
    level = BinaryTreeMath<T>::st_node_level(14);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 4) : assert(level == 4);
    level = BinaryTreeMath<T>::st_node_level(101);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 7) : assert(level == 7);
    //
    // Node Positions
    T position = 0;
    position = BinaryTreeMath<T>::st_node_position(1);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(position == 1) : assert(position == 1);
    position = BinaryTreeMath<T>::st_node_position(2);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(position == 2) : assert(position == 1);
    position = BinaryTreeMath<T>::st_node_position(3);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(position == 1) : assert(position == 2);
    position = BinaryTreeMath<T>::st_node_position(4);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(position == 3) : assert(position == 1);
    position = BinaryTreeMath<T>::st_node_position(5);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(position == 2) : assert(position == 3);
    position = BinaryTreeMath<T>::st_node_position(6);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(position == 4) : assert(position == 2);
    position = BinaryTreeMath<T>::st_node_position(7);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(position == 1) : assert(position == 4);
    position = BinaryTreeMath<T>::st_node_position(46);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(position == 31) : assert(position == 15);
    //
    // Max Position of Level (AKA node count)
    T max_position = BinaryTreeMath<T>::st_node_count_of_level(1);
    assert(max_position == 1);
    max_position = BinaryTreeMath<T>::st_node_count_of_level(2);
    assert(max_position == 2);
    max_position = BinaryTreeMath<T>::st_node_count_of_level(3);
    assert(max_position == 4);
    max_position = BinaryTreeMath<T>::st_node_count_of_level(4);
    assert(max_position == 8);
    max_position = BinaryTreeMath<T>::st_node_count_of_level(5);
    assert(max_position == 16);
    //
    // Max Node Value at Level
    mpz_class max_node = 0; // Always returns MPZ
    max_node = BinaryTreeMath<T>::st_max_node_value_at_level(1);
    assert(max_node == 0 + BinaryTreeMath<T>::get_root_value());
    max_node = BinaryTreeMath<T>::st_max_node_value_at_level(2);
    assert(max_node == 2 + BinaryTreeMath<T>::get_root_value());
    max_node = BinaryTreeMath<T>::st_max_node_value_at_level(3);
    assert(max_node == 6 + BinaryTreeMath<T>::get_root_value());
    max_node = BinaryTreeMath<T>::st_max_node_value_at_level(4);
    assert(max_node == 14 + BinaryTreeMath<T>::get_root_value());
    max_node = BinaryTreeMath<T>::st_max_node_value_at_level(5);
    assert(max_node == 30 + BinaryTreeMath<T>::get_root_value());
    //
    // Max Full Level at Node
    size_t max_level = 0; // Always returns size_t.
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(1);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 1) : assert(max_level == 1);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(4);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 2) : assert(max_level == 2);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(6);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 3) : assert(max_level == 2);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(7);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 3) : assert(max_level == 3);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(26);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 4) : assert(max_level == 4);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(29);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 4) : assert(max_level == 4);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(30);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 5) : assert(max_level == 4);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(31);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 5) : assert(max_level == 5);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(49);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 5) : assert(max_level == 5);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(63);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 6) : assert(max_level == 6);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(125);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 6) : assert(max_level == 6);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(126);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 7) : assert(max_level == 6);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(127);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 7) : assert(max_level == 7);
    //
    // First Node of Level
    assert(BinaryTreeMath<T>::st_first_node_of_level(1) == 0 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_first_node_of_level(2) == 1 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_first_node_of_level(3) == 3 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_first_node_of_level(4) == 7 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_first_node_of_level(5) == 15 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_first_node_of_level(6) == 31 + BinaryTreeMath<T>::get_root_value());
    //
    // Node Value by Position and Level
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(1, 1) == 0 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(1, 2) == 1 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(2, 2) == 2 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(1, 3) == 3 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(2, 3) == 5 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(3, 3) == 4 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(4, 3) == 6 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(1, 4) == 7 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(2, 4) == 11 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(3, 4) == 9 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(4, 4) == 13 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(5, 4) == 8 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(6, 4) == 12 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(7, 4) == 10 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(8, 4) == 14 + BinaryTreeMath<T>::get_root_value());
    //
    // Deprecated Form
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(1, 1) == 0 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(1, 2) == 1 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(2, 2) == 2 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(1, 3) == 3 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(2, 3) == 5 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(3, 3) == 4 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(4, 3) == 6 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(1, 4) == 7 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(2, 4) == 11 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(3, 4) == 9 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(4, 4) == 13 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(5, 4) == 8 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(6, 4) == 12 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(7, 4) == 10 + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(8, 4) == 14 + BinaryTreeMath<T>::get_root_value());
}


template<AnySupportedIntegral T>
void test_binary_tree_level_will_fit() {
    // Levels are now able to handle bit_width - 1 [-1 if signed].
    // 8-bit
    assert(BinaryTreeMath<uint8_t>::st_level_will_fit(4) == true);
    assert(BinaryTreeMath<uint8_t>::st_level_will_fit(7) == true);
    assert(BinaryTreeMath<uint8_t>::st_level_will_fit(8) == false);
    // 8-bit (signed)
    assert(BinaryTreeMath<int8_t>::st_level_will_fit(4) == true);
    assert(BinaryTreeMath<int8_t>::st_level_will_fit(6) == true);
    assert(BinaryTreeMath<int8_t>::st_level_will_fit(7) == false);
    assert(BinaryTreeMath<int8_t>::st_level_will_fit(8) == false);
    // 16-bit
    assert(BinaryTreeMath<uint16_t>::st_level_will_fit(4) == true);
    assert(BinaryTreeMath<uint16_t>::st_level_will_fit(15) == true);
    assert(BinaryTreeMath<uint16_t>::st_level_will_fit(16) == false);
    // 16-bit (signed)
    assert(BinaryTreeMath<int16_t>::st_level_will_fit(4) == true);
    assert(BinaryTreeMath<int16_t>::st_level_will_fit(14) == true);
    assert(BinaryTreeMath<int16_t>::st_level_will_fit(15) == false);
    assert(BinaryTreeMath<int16_t>::st_level_will_fit(16) == false);
    // 32-bit
    assert(BinaryTreeMath<uint32_t>::st_level_will_fit(10) == true);
    assert(BinaryTreeMath<uint32_t>::st_level_will_fit(31) == true);
    assert(BinaryTreeMath<uint32_t>::st_level_will_fit(32) == false);
    // 32-bit (signed)
    assert(BinaryTreeMath<int32_t>::st_level_will_fit(10) == true);
    assert(BinaryTreeMath<int32_t>::st_level_will_fit(30) == true);
    assert(BinaryTreeMath<int32_t>::st_level_will_fit(31) == false);
    assert(BinaryTreeMath<int32_t>::st_level_will_fit(32) == false);
    // 64-bit
    assert(BinaryTreeMath<uint64_t>::st_level_will_fit(18) == true);
    assert(BinaryTreeMath<uint64_t>::st_level_will_fit(63) == true);
    assert(BinaryTreeMath<uint64_t>::st_level_will_fit(64) == false);
    // 64-bit (signed)
    assert(BinaryTreeMath<int64_t>::st_level_will_fit(18) == true);
    assert(BinaryTreeMath<int64_t>::st_level_will_fit(62) == true);
    assert(BinaryTreeMath<int64_t>::st_level_will_fit(63) == false);
    assert(BinaryTreeMath<int64_t>::st_level_will_fit(64) == false);
    // MPZ Has No Ceiling
    assert(BinaryTreeMath<mpz_class>::st_level_will_fit(999) == true);
}


template<AnySupportedIntegral T>
void test_binary_tree_pruned() {
    BinaryTreeOptions opts_with_hwm_prune_without_level_prune;
    opts_with_hwm_prune_without_level_prune.prune_hwm_nodes = true;
    opts_with_hwm_prune_without_level_prune.prune_parent_levels = false;
    BinaryTreeOptions opts_with_hwm_prune_with_level_prune;
    opts_with_hwm_prune_with_level_prune.prune_hwm_nodes = true;
    opts_with_hwm_prune_with_level_prune.prune_parent_levels = true;
    BinaryTreeOptions opts_without_hwm_prune_with_level_prune;
    opts_without_hwm_prune_with_level_prune.prune_hwm_nodes = false;
    opts_without_hwm_prune_with_level_prune.prune_parent_levels = true;
    // Now test levels 1-16 and make sure the node count returned adds up (literally) to the total.
    size_t max_test_level = 16;
    size_t max_threads = 8;
    std::cout << "\n  Testing "<< max_test_level << " levels of multi-threaded deep tree building, coverage, and pruning combinations because THIS SHIT CANNOT FAIL!" << std::endl;
    for (size_t level = 1; level <= max_test_level; level++) {
        std::cout << "    Level " << level << "...";
        for (size_t thread_count = 1; thread_count <= max_threads; thread_count++) {
            // Materialized
            test_binary_tree_coverage<T, BinaryTreeMaterializedImpl<T>>(3, 1, opts_with_hwm_prune_without_level_prune);
            test_binary_tree_coverage<T, BinaryTreeMaterializedImpl<T>>(level, 1, opts_with_hwm_prune_with_level_prune);
            test_binary_tree_coverage<T, BinaryTreeMaterializedImpl<T>>(level, 1, opts_without_hwm_prune_with_level_prune);
            // Implicit trees shouldn't care about pruning options, and work without error.
            test_binary_tree_coverage<T, BinaryTreeImplicitImpl<T>>(3, 1, opts_with_hwm_prune_without_level_prune);
            test_binary_tree_coverage<T, BinaryTreeImplicitImpl<T>>(level, 1, opts_with_hwm_prune_with_level_prune);
            test_binary_tree_coverage<T, BinaryTreeImplicitImpl<T>>(level, 1, opts_without_hwm_prune_with_level_prune);
        }
        std::cout << " Okay!" << std::endl;
    }
    // Now test with level pruning too.
    // A pruned tree should always have a drastically smaller size.
    size_t levels = 16;
    float reduction_factor = 0.9;
    MaterializedBinaryTree<T> tree_raw(levels);
    MaterializedBinaryTree<T> tree_pruned(levels, opts_with_hwm_prune_without_level_prune);
    assert(tree_raw.deep_size() * reduction_factor > tree_pruned.deep_size());
}


template<AnySupportedIntegral T>
void test_binary_tree_ancestors() {
    // When disabled, they shouldn't be tracked.
    size_t levels = 5;
    BinaryTreeOptions opts;
    opts.preserve_ancestors = false;
    MaterializedBinaryTree<T> tree(levels, opts);
    assert(tree.get_ancestors().size() == 0);
    //
    // Tracking them should work.
    opts.preserve_ancestors = true;
    MaterializedBinaryTree<T> tree_with_ancestors(levels, opts);
    assert(tree_with_ancestors.get_ancestors().size() == 3);
    assert(tree_with_ancestors.get_ancestors()[0]->get_value() == 2);
    assert(tree_with_ancestors.get_ancestors()[1]->get_value() == 5);
    assert(tree_with_ancestors.get_ancestors()[2]->get_value() == 19);
    //
    // Implicit should work too.
    BinaryTreeOptions opts_implicit;
    opts_implicit.preserve_ancestors = false;
    BinaryTree<T, BinaryTreeImplicitImpl<T>> tree_implicit(levels, opts_implicit);
    assert(tree.get_ancestors().size() == 0);
    //
    // Tracking them should work.
    opts_implicit.preserve_ancestors = true;
    BinaryTree<T, BinaryTreeImplicitImpl<T>> tree_implicit_with_ancestors(levels, opts_implicit);
    assert(tree_implicit_with_ancestors.get_ancestors().size() == 3);
    assert(tree_implicit_with_ancestors.get_ancestors()[0]->get_value() == 2);
    assert(tree_implicit_with_ancestors.get_ancestors()[1]->get_value() == 5);
    assert(tree_implicit_with_ancestors.get_ancestors()[2]->get_value() == 19);
}



// Save-Load-Equality Helpers to DRY up code in test_binary_tree_save_load_equal.
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
    // Save and On-Disk Equality
    //
    // Use common file paths for all tests.
    const std::string tree_path = "__tree.bin";
    const std::string tree_duplicate_path = "__tree_duplicate.bin";
    const std::string tree_different_path = "__tree_different.bin";
    // Delete existing files if they weren't cleaned up properly last time.
    if (fs::exists(tree_path)) { fs::remove(tree_path); }
    if (fs::exists(tree_duplicate_path)) { fs::remove(tree_duplicate_path); }
    if (fs::exists(tree_different_path)) { fs::remove(tree_different_path); }
    // Save function should return true/successful.
    assert(tree.save(tree_path));
    assert(tree_duplicate.save(tree_duplicate_path));
    assert(tree_different.save(tree_different_path));
    // Open file handles to each file.
    std::ifstream fh_tree(tree_path, std::ios::binary);
    std::ifstream fh_tree_duplicate(tree_duplicate_path, std::ios::binary);
    std::ifstream fh_tree_different(tree_different_path, std::ios::binary);
    // Ensure they're actually open.
    assert(fh_tree.is_open());
    assert(fh_tree_duplicate.is_open());
    assert(fh_tree_different.is_open());
    // The two equal trees should create equal files on-disk.  The different one should be different, of course.
    assert(std::equal(std::istreambuf_iterator<char>(fh_tree), std::istreambuf_iterator<char>(), std::istreambuf_iterator<char>(fh_tree_duplicate)));
    fh_tree.clear();
    fh_tree.seekg(0, std::ios::beg);
    assert(! std::equal(std::istreambuf_iterator<char>(fh_tree), std::istreambuf_iterator<char>(), std::istreambuf_iterator<char>(fh_tree_different)));
    // Close the file handles.
    fh_tree.close();
    fh_tree_duplicate.close();
    fh_tree_different.close();

    //
    // Load and Test In-Memory Equality Again
    //
    // Make empty trees.
    BinaryTree<T, TreeType> l_tree;
    BinaryTree<T, TreeType> l_tree_duplicate;
    BinaryTree<T, TreeType> l_tree_different;
    // Load the trees from file data.
    assert(l_tree.load(tree_path));
    assert(l_tree_duplicate.load(tree_duplicate_path));
    assert(l_tree_different.load(tree_different_path));
    // Loaded trees should match the in-memory versions before we saved.
    sle_assert_trees_equal(l_tree, tree);
    sle_assert_trees_equal(l_tree_duplicate, tree_duplicate);
    sle_assert_trees_equal(l_tree_different, tree_different);

    //
    // Clean-up
    //
    // Delete the files.
    if (fs::exists(tree_path)) { fs::remove(tree_path); }
    if (fs::exists(tree_duplicate_path)) { fs::remove(tree_duplicate_path); }
    if (fs::exists(tree_different_path)) { fs::remove(tree_different_path); }
}
template<AnySupportedIntegral T>
void test_binary_tree_save_load_equal() {
    namespace fs = std::filesystem;

    // Start with smaller trees
    size_t levels = 16;

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
    const std::string tree_path = "__tree.bin";
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
}



//
// Wrapper to run all tests.
//
template<AnySupportedIntegral T>
void run_all(size_t root_value) {
    BinaryTreeMath<T>::set_root_value(root_value);

    std::cout << "test_binary_tree_save_load_equal() ..." << std::flush;
    test_binary_tree_save_load_equal<T>();
    std::cout << " passed.\n";

    std::cout << "test_binary_tree_basic_construction() ..." << std::flush;
    test_binary_tree_basic_construction<T>();
    std::cout << " passed.\n";

    std::cout << "test_binary_tree_deep_size() ..." << std::flush;
    test_binary_tree_deep_size<T>();
    std::cout << " passed.\n";

    std::cout << "test_binary_tree_generate_node_at_valid() ..." << std::flush;
    test_binary_tree_generate_node_at_valid<T>();
    std::cout << " passed.\n";

    std::cout << "test_binary_tree_generate_node_at_invalid_pos() ..." << std::flush;
    test_binary_tree_generate_node_at_invalid_pos<T>();
    std::cout << " passed.\n";

    std::cout << "test_binary_tree_coverage() ..." << std::flush;
    test_binary_tree_coverage<T, BinaryTreeMaterializedImpl<T>>();
    test_binary_tree_coverage<T, BinaryTreeImplicitImpl<T>>();
    std::cout << " passed.\n";

    std::cout << "test_binary_tree_coverage_coherency_at_scale() ..." << std::flush;
    test_binary_tree_coverage_coherency_at_scale<T>();
    std::cout << " passed.\n";

    std::cout << "test_binary_tree_ancestors() ..." << std::flush;
    test_binary_tree_ancestors<T>();
    std::cout << " passed.\n";

    std::cout << "test_binary_tree_node_count_should_match_map() ..." << std::flush;
    test_binary_tree_node_count_should_match_map<T>();
    std::cout << " passed.\n";

    std::cout << "test_binary_tree_too_many_levels() ..." << std::flush;
    test_binary_tree_too_many_levels<T>();
    std::cout << " passed.\n";

    std::cout << "test_binary_tree_multi_threaded() ..." << std::flush;
    test_binary_tree_multi_threaded<T>();
    std::cout << " passed.\n";

    std::cout << "test_binary_tree_math() ..." << std::flush;
    test_binary_tree_math<T>();
    std::cout << " passed.\n";

    std::cout << "test_binary_tree_pruned() ..." << std::flush;
    test_binary_tree_pruned<T>();
    std::cout << " passed.\n";

    std::cout << "test_binary_tree_level_will_fit() ..." << std::flush;
    test_binary_tree_level_will_fit<T>();
    std::cout << " passed.\n";

    std::cout << "test_binary_tree_save_load_equal() ..." << std::flush;
    test_binary_tree_save_load_equal<T>();
    std::cout << " passed.\n";

    BinaryTreeMath<T>::reset_root_value();
}



int main() {
    std::cout << "=====================" << std::endl;
    std::cout << "BinaryTree Tests" << std::endl;
    std::cout << "=====================" << std::endl;

    std::cout << "Performing tests with uint64_t, 0-based tree." << std::endl;
    run_all<uint64_t>(0);
    std::cout << "Performing tests with uint64_t, 1-based tree." << std::endl;
    run_all<uint64_t>(1);

    std::cout << "Performing tests with uint128_t, 0-based tree." << std::endl;
    run_all<uint128_t>(0);
    std::cout << "Performing tests with uint128_t, 1-based tree." << std::endl;
    run_all<uint128_t>(1);

    std::cout << "Performing tests with mpz_class, 0-based tree." << std::endl;
    run_all<mpz_class>(0);
    std::cout << "Performing tests with mpz_class, 1-based tree." << std::endl;
    run_all<mpz_class>(1);

    std::cout << "All BinaryTree<T> tests passed.\n";
    return 0;
}
