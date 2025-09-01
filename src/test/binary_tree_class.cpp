#include <cassert>
#include <cstdint>
#include <iostream>
#include <gmpxx.h>
#include <stdexcept>
#include "../collatz/binary_tree.hpp"
#include "../collatz/binary_tree_math.hpp"
#include "../collatz/binary_tree_coverage.hpp"


//
// All of these tests need to work with native integrals and GMP.
// All of these tests need to work with 0-rooted and 1-rooted (traditional) binary trees.
//
// WHEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE
//


template<AnySupportedIntegral T>
void test_binary_tree_basic_construction() {
    BinaryTree<T> tree(3);
    // Build extras to ensure isolation, double-freeing, etc are all good.  These will destruct at function end (end of scope).
    BinaryTree<T> tree2(2);
    BinaryTree<T> tree3(4);
    BinaryTree<T> tree4(5);
    BinaryTree<T> tree5(6);
    BinaryTree<T> tree6(7);
    assert(tree.get_level_count() == 3);
    const auto& map = tree.get_level_map();
    //
    // Level 0: root
    assert(map.at(0).size() == 1);
    assert(map.at(0)[0]->get_value() == 0 + BinaryTreeMath<T>::get_root_value());
    //
    // Level 1: 2 children (0 + 1 + Offset, 0 + 2 + Offset)
    assert(map.at(1).size() == 2);
    assert(map.at(1)[0]->get_value() == 1 + BinaryTreeMath<T>::get_root_value());
    assert(map.at(1)[1]->get_value() == 2 + BinaryTreeMath<T>::get_root_value());
    //
    // Level 2: each child has 2 children, total 4
    assert(map.at(2).size() == 4);
    //
    // Level 3: 8 nodes expected
    assert(map.at(3).size() == 8);
}


template<AnySupportedIntegral T>
void test_binary_tree_deep_size() {
    BinaryTree<T> tree(2);
    size_t size = tree.deep_size();
    assert(size > sizeof(tree)); // Make sure something was counted
}


template<AnySupportedIntegral T>
void test_binary_tree_generate_node_at_valid() {
    Node<T>* node = BinaryTree<T>::st_generate_node_at(2, 1);
    assert(node != nullptr);
    assert(node->get_value() == 3 + BinaryTreeMath<T>::get_root_value());
    delete node;
    node = BinaryTree<T>::st_generate_node_at(2, 4);
    assert(node != nullptr);
    assert(node->get_value() == 6 + BinaryTreeMath<T>::get_root_value());
    delete node;
    node = BinaryTree<T>::st_generate_node_at(3, 4);
    assert(node != nullptr);
    assert(node->get_value() == 13 + BinaryTreeMath<T>::get_root_value());
    delete node;
    node = BinaryTree<T>::st_generate_node_at(5, 12);
    assert(node != nullptr);
    assert(node->get_value() == 57 + BinaryTreeMath<T>::get_root_value());
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

    try {
        BinaryTree<T>::st_generate_node_at(3, 9); // 2^3 = 8 max
        assert(false); // Should throw
    } catch (const std::out_of_range& e) {
        assert(std::string(e.what()).find("outside of a level") != std::string::npos);
    }
}


template<AnySupportedIntegral T>
void test_binary_tree_coverage(size_t levels = 16, size_t threads = 1, const BinaryTreeOptions& opts = BinaryTree<T>::DEFAULT_OPTS) {
    // Set threads.
    omp_set_num_threads(threads);
    //
    // Build trees.
    BinaryTree<T> tree(levels, opts);
    BinaryTreeCoverage<T> global_coverage;
    T target_total = 0;
    for (size_t level=BinaryTreeMath<T>::get_base_level(); level<=tree.get_level_count(); level++) {
        target_total = (T{1} << level);
        const BinaryTreeCoverage<T>* coverage = &tree.get_coverage_map().find(level)->second;
        global_coverage.add_covered(coverage->get_covered());
        global_coverage.add_total(coverage->get_total());
        assert(coverage->get_covered() == BinaryTreeCoverageConstants::get_known_coverage(level));
        assert(coverage->get_total() == target_total);
    }
    assert(global_coverage.get_covered() == BinaryTreeCoverageConstants::get_known_coverage_sum_to_level(levels));
    assert(global_coverage.get_total() == BinaryTreeCoverageConstants::get_total_sum_to_level(BinaryTreeMath<T>::get_base_level(), levels));
    // Tree should always have: 2^(level+1) - 2 nodes total if it's not pruned.
    // We allow pruning of HWM nodes and/or levels.  Need tests for each.
    //
    // Option 1: false and false
    // This is a tree with all its nodes.  Counts should line up without adjustment.
    if (!tree.is_pruning_hwm_nodes() && !tree.is_pruning_parent_levels()) {
        assert(tree.node_count() == (T{1} << (tree.get_level_count() + 1)) - 1 - BinaryTreeMath<T>::get_offset());
    }
    // Option 2: true and false
    // HWM pruning is on, but levels are not erased.  Keeps a fulled pared list of non-HWM nodes.  Just add covered back in.
    if (tree.is_pruning_hwm_nodes() && !tree.is_pruning_parent_levels()) {
        T node_count_summary = tree.node_count();
        for (size_t level = BinaryTreeMath<T>::get_base_level(); level <= tree.get_level_count(); level++) {
            node_count_summary += tree.get_coverage_map().find(level)->second.get_covered();
        }
        assert(node_count_summary == BinaryTreeCoverageConstants::get_total_sum_to_level(BinaryTreeMath<T>::get_base_level(), levels));
    }
    // Option 3: false and true
    // HWM pruning is off, but parent levels are removed.  Keeps a full final level and nothing more.  Add previous known coverage.
    if (!tree.is_pruning_hwm_nodes() && tree.is_pruning_parent_levels()) {
        T node_count_summary = tree.node_count() + BinaryTreeCoverageConstants::get_total_sum_to_level(BinaryTreeMath<T>::get_base_level(), levels - 1);
        assert(node_count_summary == BinaryTreeCoverageConstants::get_total_sum_to_level(BinaryTreeMath<T>::get_base_level(), levels));
    }
    // Option 4: true and true
    // HWM pruning is on, and parent levels are removed.  Keeps a pared-down final level of non-HWM nodes.
    if (tree.is_pruning_hwm_nodes() && tree.is_pruning_parent_levels()) {
        T node_count_plus_uncovered = tree.node_count();
        for (size_t level = BinaryTreeMath<T>::get_base_level(); level < tree.get_level_count(); level++) {
            node_count_plus_uncovered += tree.get_coverage_map().find(level)->second.get_uncovered();
        }
        assert(node_count_plus_uncovered + BinaryTreeCoverageConstants::get_known_coverage_sum_to_level(levels) == BinaryTreeCoverageConstants::get_total_sum_to_level(BinaryTreeMath<T>::get_base_level(), levels));
    }
}


template<AnySupportedIntegral T>
void test_binary_tree_node_count_should_match_map() {
    size_t levels = 4;
    BinaryTree<T> tree(levels);
    T expected_count = (T{2} << levels) - 1 - BinaryTreeMath<T>::get_offset();
    size_t map_count = 0;
    const auto& map = tree.get_level_map();
    for (size_t i = BinaryTreeMath<T>::get_base_level(); i <= tree.get_level_count(); i++) {
        map_count += map.at(i).size();
    }
    assert(tree.node_count() == expected_count);
    assert(map_count == expected_count);
}


template<AnySupportedIntegral T>
void test_binary_tree_too_many_levels() {
    // 8 bit can't build a 16 level tree
    try {
        BinaryTree<uint8_t> tree(16);
        assert(false); // Should throw
    } catch (const std::out_of_range& e) {
        assert(std::string(e.what()).find("Cannot build a BinaryTree with 16") != std::string::npos);
    }
    // A 32-bit tree type can.
    BinaryTree<uint32_t> tree(16);
    // An 8 bit can, however,  handle a 3-level tree.
    BinaryTree<uint8_t> tree2(3);
    // Adding another level should break.
    try {
        tree2.add_level();
        assert(false); // Should throw
    } catch (const std::out_of_range& e) {
        assert(std::string(e.what()).find("Cannot build a BinaryTree with 4") != std::string::npos);
    }
}


template<AnySupportedIntegral T>
void test_binary_tree_multi_threaded() {
    size_t max_threads = 4;
    for (size_t threads = 1; threads <= max_threads; threads++) {
        test_binary_tree_coverage<T>(16, threads);
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
    level = BinaryTreeMath<T>::st_node_level(4);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 2) : assert(level == 2);
    level = BinaryTreeMath<T>::st_node_level(101);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 6) : assert(level == 6);
    level = BinaryTreeMath<T>::st_node_level(1);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 1) : assert(level == 0);
    level = BinaryTreeMath<T>::st_node_level(2);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 1) : assert(level == 1);
    level = BinaryTreeMath<T>::st_node_level(3);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 2) : assert(level == 1);
    level = BinaryTreeMath<T>::st_node_level(7);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 3) : assert(level == 2);
    level = BinaryTreeMath<T>::st_node_level(14);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 3) : assert(level == 3);
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
    // Max Position of Level
    T max_position = BinaryTreeMath<T>::st_max_position_of_level(0);
    assert(max_position == 1);
    max_position = BinaryTreeMath<T>::st_max_position_of_level(1);
    assert(max_position == 2);
    max_position = BinaryTreeMath<T>::st_max_position_of_level(2);
    assert(max_position == 4);
    max_position = BinaryTreeMath<T>::st_max_position_of_level(3);
    assert(max_position == 8);
    max_position = BinaryTreeMath<T>::st_max_position_of_level(4);
    assert(max_position == 16);
    //
    // Max Node Value at Level
    mpz_class max_node = 0; // Always returns MPZ
    max_node = BinaryTreeMath<T>::st_max_node_value_at_level(0);
    assert(max_node == 0 + BinaryTreeMath<T>::get_root_value());
    max_node = BinaryTreeMath<T>::st_max_node_value_at_level(1);
    assert(max_node == 2 + BinaryTreeMath<T>::get_root_value());
    max_node = BinaryTreeMath<T>::st_max_node_value_at_level(2);
    assert(max_node == 6 + BinaryTreeMath<T>::get_root_value());
    max_node = BinaryTreeMath<T>::st_max_node_value_at_level(3);
    assert(max_node == 14 + BinaryTreeMath<T>::get_root_value());
    max_node = BinaryTreeMath<T>::st_max_node_value_at_level(4);
    assert(max_node == 30 + BinaryTreeMath<T>::get_root_value());
    //
    // Max Full Level at Node
    size_t max_level = 0; // Always returns size_t.
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(1);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 0) : assert(max_level == 0);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(4);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 1) : assert(max_level == 1);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(6);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 2) : assert(max_level == 1);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(7);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 2) : assert(max_level == 2);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(26);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 3) : assert(max_level == 3);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(29);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 3) : assert(max_level == 3);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(30);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 4) : assert(max_level == 3);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(31);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 4) : assert(max_level == 4);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(49);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 4) : assert(max_level == 4);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(63);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 5) : assert(max_level == 5);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(125);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 5) : assert(max_level == 5);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(126);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 6) : assert(max_level == 5);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(127);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 6) : assert(max_level == 6);
    //
    // First Node of Level
    T first_node = BinaryTreeMath<T>::st_first_node_of_level(0);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(first_node == 0) : assert(first_node == 1);
    first_node = BinaryTreeMath<T>::st_first_node_of_level(1);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(first_node == 1) : assert(first_node == 2);
    first_node = BinaryTreeMath<T>::st_first_node_of_level(2);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(first_node == 3) : assert(first_node == 4);
    first_node = BinaryTreeMath<T>::st_first_node_of_level(3);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(first_node == 7) : assert(first_node == 8);
    first_node = BinaryTreeMath<T>::st_first_node_of_level(4);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(first_node == 15) : assert(first_node == 16);
    first_node = BinaryTreeMath<T>::st_first_node_of_level(5);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(first_node == 31) : assert(first_node == 32);
    //
    // Node Value by Position and Level
    T node_value = BinaryTreeMath<T>::st_node_value_by_position_and_level(1, 1);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(node_value == 1) : assert(node_value == 2);
    node_value = BinaryTreeMath<T>::st_node_value_by_position_and_level(2, 1);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(node_value == 2) : assert(node_value == 3);
    node_value = BinaryTreeMath<T>::st_node_value_by_position_and_level(1, 2);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(node_value == 3) : assert(node_value == 4);
    node_value = BinaryTreeMath<T>::st_node_value_by_position_and_level(2, 2);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(node_value == 5) : assert(node_value == 6);
    node_value = BinaryTreeMath<T>::st_node_value_by_position_and_level(7, 5);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(node_value == 43) : assert(node_value == 44);
    //
    // Deprecated Form
    node_value = BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(1, 1);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(node_value == 1) : assert(node_value == 2);
    node_value = BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(2, 1);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(node_value == 2) : assert(node_value == 3);
    node_value = BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(1, 2);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(node_value == 3) : assert(node_value == 4);
    node_value = BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(2, 2);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(node_value == 5) : assert(node_value == 6);
    node_value = BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(7, 5);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(node_value == 43) : assert(node_value == 44);
}


template<AnySupportedIntegral T>
void test_binary_tree_level_will_fit() {
    // Level Will Fit
    // 8-bit
    assert(BinaryTreeMath<uint8_t>::st_level_will_fit(3) == true);
    assert(BinaryTreeMath<uint8_t>::st_level_will_fit(4) == false);
    // 16-bit
    assert(BinaryTreeMath<uint16_t>::st_level_will_fit(4) == true);
    assert(BinaryTreeMath<uint16_t>::st_level_will_fit(8) == true);
    assert(BinaryTreeMath<uint16_t>::st_level_will_fit(9) == false);
    // 32-bit
    assert(BinaryTreeMath<uint32_t>::st_level_will_fit(9) == true);
    assert(BinaryTreeMath<uint32_t>::st_level_will_fit(16) == true);
    assert(BinaryTreeMath<uint32_t>::st_level_will_fit(17) == false);
    // 64-bit
    assert(BinaryTreeMath<uint64_t>::st_level_will_fit(17) == true);
    assert(BinaryTreeMath<uint64_t>::st_level_will_fit(32) == true);
    assert(BinaryTreeMath<uint64_t>::st_level_will_fit(33) == false);
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
    std::cout << "\n  Testing "<< max_test_level << " levels of deep tree build, coverage, and pruning combinations because THIS SHIT CANNOT FAIL!" << std::endl;
    for (size_t level = 1; level <= max_test_level; level++) {
        std::cout << "    Level " << level << "...";
        test_binary_tree_coverage<T>(3, 1, opts_with_hwm_prune_without_level_prune);
        test_binary_tree_coverage<T>(level, 1, opts_with_hwm_prune_with_level_prune);
        test_binary_tree_coverage<T>(level, 1, opts_without_hwm_prune_with_level_prune);
        std::cout << " Okay!" << std::endl;
    }
    // Now test with level pruning too.
    // A pruned tree should always have a drastically smaller size.
    size_t levels = 16;
    float reduction_factor = 0.9;
    BinaryTree<T> tree_raw(levels);
    BinaryTree<T> tree_pruned(levels, opts_with_hwm_prune_without_level_prune);
    assert(tree_raw.deep_size() * reduction_factor > tree_pruned.deep_size());
}


template<AnySupportedIntegral T>
void test_binary_tree_ancestors() {
    // When disabled, they shouldn't be tracked.
    size_t levels = 4;
    BinaryTreeOptions opts;
    opts.preserve_ancestors = false;
    BinaryTree<T> tree(levels, opts);
    assert(tree.get_ancestors().size() == 0);
    //
    // Tracking them should work.
    opts.preserve_ancestors = true;
    BinaryTree<T> tree_with_ancestors(levels, opts);
    assert(tree_with_ancestors.get_ancestors().size() == 3);
    assert(tree_with_ancestors.get_ancestors()[0]->get_value() == 2);
    assert(tree_with_ancestors.get_ancestors()[1]->get_value() == 5);
    assert(tree_with_ancestors.get_ancestors()[2]->get_value() == 19);
}




//
// Wrapper to run all tests.
//
template<AnySupportedIntegral T>
void run_all(size_t root_value) {
    BinaryTreeMath<T>::set_root_value(root_value);

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
    test_binary_tree_coverage<T>();
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

    BinaryTreeMath<T>::reset_root_value();
}



int main() {
    std::cout << "Performing tests with uint64_t, 0-based tree." << std::endl;
    run_all<uint64_t>(0);
    std::cout << "Performing tests with uint64_t, 1-based tree." << std::endl;
    run_all<uint64_t>(1);

    std::cout << "Performing tests with mpz_class, 0-based tree." << std::endl;
    run_all<mpz_class>(0);
    std::cout << "Performing tests with mpz_class, 1-based tree." << std::endl;
    run_all<mpz_class>(1);

    std::cout << "All BinaryTree<T> tests passed.\n";
    return 0;
}
