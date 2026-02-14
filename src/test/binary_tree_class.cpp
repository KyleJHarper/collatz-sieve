#include <cassert>
#include <cstdint>
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
    BinaryTree<T> tree(5);
    // Build extras to ensure isolation, double-freeing, etc are all good.  These will destruct at function end (end of scope).
    BinaryTree<T> tree2(2);
    BinaryTree<T> tree3(4);
    BinaryTree<T> tree4(5);
    BinaryTree<T> tree5(6);
    BinaryTree<T> tree6(7);
    assert(tree.get_root_node()->get_value() == BinaryTreeMath<T>::get_root_value());
    assert(tree.get_level_count() == 5);
    tree.assert_materialized("binary_tree_class");
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
    // Build an Implicit tree now.
    BinaryTreeOptions implicit_opts;
    implicit_opts.tree_type = BinaryTreeType::IMPLICIT;
    BinaryTree<T> implicit_tree(5, implicit_opts);
    assert(implicit_tree.get_level_count() == 5);
    implicit_tree.assert_implicit("binary_tree_class");
    const auto& covered_intervals = implicit_tree.get_covered_intervals();
    //
    // Implicit trees do not have a level map.  Instead, we verify their covered intervals to ensure the positions align with known
    // subtrees that "pruning" wouldn't caught in a Materialized tree.
    // Positions shift based on the root node being 0-based or 1-based, fyi...
    // REMEMBER THESE ARE SORTED!  CHANGES THE INDEX THEY'RE ON!
    size_t root_value = BinaryTreeMath<T>::get_root_value();
    if (root_value == 0) {
        //
        // Level 1: root
        level = 1;
        assert(implicit_tree.get_root_node()->get_value() == 0 + BinaryTreeMath<T>::get_root_value());
        //
        // Level 2: 2 children, and only one of them is covered (total 1).  It's the number 2, which is in position 2.
        level = 2;
        assert(covered_intervals.at(level).size() == 1);
        assert(covered_intervals.at(level)[0].start == 2);
        assert(covered_intervals.at(level)[0].end == 2);
        //
        // Level 3: 4 children, and only one of them is covered (total 2).  It's the number 5, which is in position 2.
        level = 3;
        assert(covered_intervals.at(level).size() == 2);
        assert(covered_intervals.at(level)[0].start == 2);
        assert(covered_intervals.at(level)[0].end == 2);
        // Now scaled intervals...
        assert(covered_intervals.at(level)[1].start == 3);
        assert(covered_intervals.at(level)[1].end == 4);
        //
        // Level 4: 8 children, none are covered (total remains 2).
        level = 4;
        assert(covered_intervals.at(level).size() == 2);
        // Now scaled intervals...
        assert(covered_intervals.at(level)[0].start == 3);
        assert(covered_intervals.at(level)[0].end == 4);
        assert(covered_intervals.at(level)[1].start == 5);
        assert(covered_intervals.at(level)[1].end == 8);
        //
        // Level 5: 16 children, one is covered (total 3).  It's the number 19, which is in position 3.
        level = 5;
        assert(covered_intervals.at(level).size() == 3);
        assert(covered_intervals.at(level)[0].start == 3);
        assert(covered_intervals.at(level)[0].end == 3);
        // Now scaled intervals...
        assert(covered_intervals.at(level)[1].start == 5);
        assert(covered_intervals.at(level)[1].end == 8);
        assert(covered_intervals.at(level)[2].start == 9);
        assert(covered_intervals.at(level)[2].end == 16);
    } else {
        //
        // Level 1: root
        level = 1;
        assert(implicit_tree.get_root_node()->get_value() == 0 + BinaryTreeMath<T>::get_root_value());
        //
        // Level 2: 2 children, and only one of them is covered (total 1).  It's the number 2, which is in position 1.
        level = 2;
        assert(covered_intervals.at(level).size() == 1);
        assert(covered_intervals.at(level)[0].start == 1);
        assert(covered_intervals.at(level)[0].end == 1);
        //
        // Level 3: 4 children, and only one of them is covered (total 2).  It's the number 5, which is in position 3.
        level = 3;
        assert(covered_intervals.at(level).size() == 2);
        assert(covered_intervals.at(level)[1].start == 3);
        assert(covered_intervals.at(level)[1].end == 3);
        // Now scaled intervals... (pay attention to indexes!)
        assert(covered_intervals.at(level)[0].start == 1);
        assert(covered_intervals.at(level)[0].end == 2);
        //
        // Level 4: 8 children, none are covered (total remains 2).
        level = 4;
        assert(covered_intervals.at(level).size() == 2);
        // Now scaled intervals... (pay attention to indexes!)
        assert(covered_intervals.at(level)[0].start == 1);
        assert(covered_intervals.at(level)[0].end == 4);
        assert(covered_intervals.at(level)[1].start == 5);
        assert(covered_intervals.at(level)[1].end == 6);
        //
        // Level 5: 16 children, one is covered (total 3).  It's the number 19, which is in position 13.
        level = 5;
        assert(covered_intervals.at(level).size() == 3);
        assert(covered_intervals.at(level)[2].start == 13);
        assert(covered_intervals.at(level)[2].end == 13);
        // Now scaled intervals... (pay attention to indexes!)
        assert(covered_intervals.at(level)[0].start == 1);
        assert(covered_intervals.at(level)[0].end == 8);
        assert(covered_intervals.at(level)[1].start == 9);
        assert(covered_intervals.at(level)[1].end == 12);
    }
}


template<AnySupportedIntegral T>
void test_binary_tree_deep_size() {
    BinaryTreeOptions opts;
    opts.tree_type = BinaryTreeType::MATERIALIZED;
    BinaryTree<T> tree(2, opts);
    size_t size = tree.deep_size();
    assert(size > sizeof(tree)); // Make sure something was counted
    opts.tree_type = BinaryTreeType::IMPLICIT;
    BinaryTree<T> tree_implicit(2, opts);
    size_t size_implicit = tree_implicit.deep_size();
    assert(size_implicit > sizeof(tree_implicit)); // Make sure something was counted
}


template<AnySupportedIntegral T>
void test_binary_tree_generate_node_at_valid() {
    // Materialized Tree
    size_t root_value = BinaryTreeMath<T>::get_root_value();
    Node<T>* node = nullptr;
    node = BinaryTreeMaterialized<T>::st_generate_node_at(2, 1);
    assert(node != nullptr);
    assert(node->get_value() == 1 + root_value);
    delete node;
    node = BinaryTreeMaterialized<T>::st_generate_node_at(3, 1);
    assert(node != nullptr);
    assert(node->get_value() == 3 + root_value);
    delete node;
    node = BinaryTreeMaterialized<T>::st_generate_node_at(3, 4);
    assert(node != nullptr);
    assert(node->get_value() == 6 + root_value);
    delete node;
    node = BinaryTreeMaterialized<T>::st_generate_node_at(4, 4);
    assert(node != nullptr);
    assert(node->get_value() == 13 + root_value);
    delete node;
    node = BinaryTreeMaterialized<T>::st_generate_node_at(4, 5);
    assert(node != nullptr);
    assert(node->get_value() == 8 + root_value);
    delete node;
    node = BinaryTreeMaterialized<T>::st_generate_node_at(4, 8);
    assert(node != nullptr);
    assert(node->get_value() == 14 + root_value);
    delete node;
    node = BinaryTreeMaterialized<T>::st_generate_node_at(5, 12);
    assert(node != nullptr);
    assert(node->get_value() == 28 + root_value);
    delete node;
    //
    // Note: implicit trees cannot create material nodes, so no test here.
}


template<AnySupportedIntegral T>
void test_binary_tree_generate_node_at_invalid_pos() {
    try {
        BinaryTreeMaterialized<T>::st_generate_node_at(3, 0);
        assert(false); // Should throw
    } catch (const std::out_of_range& e) {
        assert(std::string(e.what()).find("position 0") != std::string::npos);
    }
    //
    try {
        BinaryTreeMaterialized<T>::st_generate_node_at(3, 9); // 2^3 = 8 max
        assert(false); // Should throw
    } catch (const std::out_of_range& e) {
        assert(std::string(e.what()).find("outside of a level") != std::string::npos);
    }
    //
    // Implicit trees cannot generate nodes.
}


template<AnySupportedIntegral T>
void test_binary_tree_coverage(size_t levels = 16, size_t threads = 1, const BinaryTreeOptions& opts = BinaryTree<T>::DEFAULT_OPTS) {
    // Set threads.
    omp_set_num_threads(threads);
    //
    // Build trees.
    BinaryTree<T> tree(levels, opts);
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
    assert(global_coverage.get_covered() == BinaryTreeCoverageConstants::get_known_coverage_sum_to_level<T>(levels));
    assert(global_coverage.get_total() == BinaryTreeCoverageConstants::get_total_sum_of_levels<T>(1, levels));
    //
    // Materialized trees have pruning options.
    if (opts.tree_type == BinaryTreeType::MATERIALIZED) {
        // Tree should always have: 2^(level+1) - 2 nodes total if it's not pruned.
        // We allow pruning of HWM nodes and/or levels.  Need tests for each.
        //
        // Option 1: false and false
        // This is a tree with all its nodes.  Counts should line up without adjustment.
        if (!tree.is_pruning_hwm_nodes() && !tree.is_pruning_parent_levels()) {
            assert(tree.node_count() == (T{1} << (tree.get_level_count())) - 1);
        }
        // Option 2: true and false
        // HWM pruning is on, but levels are not erased.  Keeps a fulled pared list of non-HWM nodes.  Just add covered back in.
        if (tree.is_pruning_hwm_nodes() && !tree.is_pruning_parent_levels()) {
            T node_count_summary = tree.node_count();
            for (size_t level = 1; level <= tree.get_level_count(); level++) {
                node_count_summary += tree.get_coverage_map().find(level)->second.get_covered();
            }
            assert(node_count_summary == BinaryTreeCoverageConstants::get_total_sum_of_levels<T>(1, levels));
        }
        // Option 3: false and true
        // HWM pruning is off, but parent levels are removed.  Keeps a full final level and nothing more.  Add previous known coverage.
        if (!tree.is_pruning_hwm_nodes() && tree.is_pruning_parent_levels()) {
            T node_count_summary = tree.node_count() + BinaryTreeCoverageConstants::get_total_sum_of_levels<T>(1, levels - 1);
            assert(node_count_summary == BinaryTreeCoverageConstants::get_total_sum_of_levels<T>(1, levels));
        }
        // Option 4: true and true
        // HWM pruning is on, and parent levels are removed.  Keeps a pared-down final level of non-HWM nodes.
        if (tree.is_pruning_hwm_nodes() && tree.is_pruning_parent_levels()) {
            T node_count_plus_uncovered = tree.node_count();
            for (size_t level = 1; level < tree.get_level_count(); level++) {
                node_count_plus_uncovered += tree.get_coverage_map().find(level)->second.get_uncovered();
            }
            assert(node_count_plus_uncovered + BinaryTreeCoverageConstants::get_known_coverage_sum_to_level<T>(levels) == BinaryTreeCoverageConstants::get_total_sum_of_levels<T>(1, levels));
        }
    }
}


template<AnySupportedIntegral T>
void test_binary_tree_coverage_coherency_at_scale() {
    BinaryTreeOptions opts;
    opts.tree_type = BinaryTreeType::IMPLICIT;
    if constexpr(NativeIntegral<T>) {
        // Level 25 should work.
        test_binary_tree_coverage<T>(25, 8, opts);
        // Level 26 should break.
        try {
            test_binary_tree_coverage<T>(26, 8, opts);
            assert(false); // Should throw
        } catch (const std::overflow_error& e) {
            assert(std::string(e.what()).find("Overflow in CollatzAffineMap calculate() method") != std::string::npos);
        }
    } else if constexpr(ExtendedIntegral<T>) {
        // Level 32 should work.
        test_binary_tree_coverage<T>(32, 8, opts);
        //TODO First level to fail is unknown.
    }
    // GMP has no overflow potential.
}


template<AnySupportedIntegral T>
void test_binary_tree_node_count_should_match_map() {
    size_t levels = 5;
    BinaryTree<T> tree(levels);
    T expected_count = (T{1} << levels) - 1;
    size_t map_count = 0;
    const auto& map = tree.get_level_map();
    for (size_t i = 1; i <= tree.get_level_count(); i++) {
        map_count += map.at(i).size();
    }
    assert(tree.node_count() == expected_count);
    assert(map_count == expected_count);
    //
    // No implicit tree test here because there's no map.
}


template<AnySupportedIntegral T>
void test_binary_tree_too_many_levels() {
    BinaryTreeOptions opts;
    //
    // Materialized
    opts.tree_type = BinaryTreeType::MATERIALIZED;
    // 8 bit can't build a 16 level tree
    try {
        BinaryTree<uint8_t> tree(16, opts);
        assert(false); // Should throw
    } catch (const std::out_of_range& e) {
        assert(std::string(e.what()).find("Cannot build a BinaryTree with 16") != std::string::npos);
    }
    // A 32-bit tree type can.
    BinaryTree<uint32_t> tree(16, opts);
    // An 8 bit can, however,  handle a 4-level tree.
    BinaryTree<uint8_t> tree2(4, opts);
    // Adding another level should break.
    try {
        tree2.add_level();
        assert(false); // Should throw
    } catch (const std::out_of_range& e) {
        assert(std::string(e.what()).find("Cannot build a BinaryTree with 5") != std::string::npos);
    }
    //
    // Implicit
    opts.tree_type = BinaryTreeType::IMPLICIT;
    // 8 bit can't build a 16 level tree
    try {
        BinaryTree<uint8_t> tree_implicit(16, opts);
        assert(false); // Should throw
    } catch (const std::out_of_range& e) {
        assert(std::string(e.what()).find("Cannot build a BinaryTree with 16") != std::string::npos);
    }
    // A 32-bit tree type can.
    BinaryTree<uint32_t> tree_implicit(16, opts);
    // An 8 bit can, however,  handle a 4-level tree.
    BinaryTree<uint8_t> tree2_implicit(4, opts);
    // Adding another level should break.
    try {
        tree2_implicit.add_level();
        assert(false); // Should throw
    } catch (const std::out_of_range& e) {
        assert(std::string(e.what()).find("Cannot build a BinaryTree with 5") != std::string::npos);
    }
}


template<AnySupportedIntegral T>
void test_binary_tree_multi_threaded() {
    BinaryTreeOptions opts = BinaryTree<T>::DEFAULT_OPTS;
    size_t max_threads = 4;
    // Materialized
    opts.tree_type = BinaryTreeType::MATERIALIZED;
    for (size_t threads = 1; threads <= max_threads; threads++) {
        test_binary_tree_coverage<T>(16, threads, opts);
    }
    // Implicit
    opts.tree_type = BinaryTreeType::IMPLICIT;
    for (size_t threads = 1; threads <= max_threads; threads++) {
        test_binary_tree_coverage<T>(16, threads, opts);
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
    // Level Will Fit
    // 8-bit
    assert(BinaryTreeMath<uint8_t>::st_level_will_fit(4) == true);
    assert(BinaryTreeMath<uint8_t>::st_level_will_fit(5) == false);
    // 16-bit
    assert(BinaryTreeMath<uint16_t>::st_level_will_fit(4) == true);
    assert(BinaryTreeMath<uint16_t>::st_level_will_fit(9) == true);
    assert(BinaryTreeMath<uint16_t>::st_level_will_fit(10) == false);
    // 32-bit
    assert(BinaryTreeMath<uint32_t>::st_level_will_fit(10) == true);
    assert(BinaryTreeMath<uint32_t>::st_level_will_fit(17) == true);
    assert(BinaryTreeMath<uint32_t>::st_level_will_fit(18) == false);
    // 64-bit
    assert(BinaryTreeMath<uint64_t>::st_level_will_fit(18) == true);
    assert(BinaryTreeMath<uint64_t>::st_level_will_fit(33) == true);
    assert(BinaryTreeMath<uint64_t>::st_level_will_fit(34) == false);
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
    BinaryTreeOptions opts_implicit_with_hwm_prune_without_level_prune;
    opts_implicit_with_hwm_prune_without_level_prune.tree_type = BinaryTreeType::IMPLICIT;
    opts_implicit_with_hwm_prune_without_level_prune.prune_hwm_nodes = true;
    opts_implicit_with_hwm_prune_without_level_prune.prune_parent_levels = false;
    BinaryTreeOptions opts_implicit_with_hwm_prune_with_level_prune;
    opts_implicit_with_hwm_prune_with_level_prune.tree_type = BinaryTreeType::IMPLICIT;
    opts_implicit_with_hwm_prune_with_level_prune.prune_hwm_nodes = true;
    opts_implicit_with_hwm_prune_with_level_prune.prune_parent_levels = true;
    BinaryTreeOptions opts_implicit_without_hwm_prune_with_level_prune;
    opts_implicit_without_hwm_prune_with_level_prune.tree_type = BinaryTreeType::IMPLICIT;
    opts_implicit_without_hwm_prune_with_level_prune.prune_hwm_nodes = false;
    opts_implicit_without_hwm_prune_with_level_prune.prune_parent_levels = true;
    // Now test levels 1-16 and make sure the node count returned adds up (literally) to the total.
    size_t max_test_level = 16;
    size_t max_threads = 4;
    std::cout << "\n  Testing "<< max_test_level << " levels of multi-threaded deep tree building, coverage, and pruning combinations because THIS SHIT CANNOT FAIL!" << std::endl;
    for (size_t level = 1; level <= max_test_level; level++) {
        std::cout << "    Level " << level << "...";
        for (size_t thread_count = 1; thread_count <= max_threads; thread_count++) {
            // Materialized
            test_binary_tree_coverage<T>(3, 1, opts_with_hwm_prune_without_level_prune);
            test_binary_tree_coverage<T>(level, 1, opts_with_hwm_prune_with_level_prune);
            test_binary_tree_coverage<T>(level, 1, opts_without_hwm_prune_with_level_prune);
            // Implicit trees shouldn't care about pruning options, and work without error.
            test_binary_tree_coverage<T>(3, 1, opts_implicit_with_hwm_prune_without_level_prune);
            test_binary_tree_coverage<T>(level, 1, opts_implicit_with_hwm_prune_with_level_prune);
            test_binary_tree_coverage<T>(level, 1, opts_implicit_without_hwm_prune_with_level_prune);
        }
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
    size_t levels = 5;
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
    //
    // Implicit should work too.
    BinaryTreeOptions opts_implicit;
    opts_implicit.tree_type = BinaryTreeType::IMPLICIT;
    opts_implicit.preserve_ancestors = false;
    BinaryTree<T> tree_implicit(levels, opts_implicit);
    assert(tree.get_ancestors().size() == 0);
    //
    // Tracking them should work.
    opts_implicit.preserve_ancestors = true;
    BinaryTree<T> tree_implicit_with_ancestors(levels, opts_implicit);
    assert(tree_implicit_with_ancestors.get_ancestors().size() == 3);
    assert(tree_implicit_with_ancestors.get_ancestors()[0]->get_value() == 2);
    assert(tree_implicit_with_ancestors.get_ancestors()[1]->get_value() == 5);
    assert(tree_implicit_with_ancestors.get_ancestors()[2]->get_value() == 19);
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

    std::cout << "test_binary_tree_coverage_coherency_at_scale() ..." << std::flush;
    test_binary_tree_coverage_coherency_at_scale<T>();
    std::cout << " passed.\n";

    std::cout << "test_binary_tree_ancestors() ..." << std::flush;
    test_binary_tree_ancestors<T>();
    std::cout << " passed.\n";

    std::cout << "test_binary_tree_node_count_should_match_map() ..." << std::flush;
    test_binary_tree_node_count_should_match_map<T>();
    std::cout << " passed.\n";

    // std::cout << "test_binary_tree_too_many_levels() ..." << std::flush;
    // test_binary_tree_too_many_levels<T>();
    // std::cout << " passed.\n";

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
