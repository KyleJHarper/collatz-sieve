#include <cassert>
#include <cstdint>
#include <iostream>
#include <gmpxx.h>
#include <stdexcept>
#include "../collatz/binary_tree.hpp" // Include your BinaryTree and Node classes
#include "../collatz/binary_tree_coverage.hpp"


void test_binary_tree_basic_construction() {
    BinaryTree<uint64_t> tree(3);
    // Build extras to ensure isolation, double-freeing, etc are all good.  These will destruct at function end (end of scope).
    BinaryTree<uint64_t> tree2(2);
    BinaryTree<uint64_t> tree3(4);
    BinaryTree<uint64_t> tree4(5);
    BinaryTree<uint64_t> tree5(6);
    BinaryTree<uint64_t> tree6(7);
    assert(tree.get_level_count() == 3);
    const auto& map = tree.get_level_map();

    // Level 0: root
    assert(map.at(0).size() == 1);
    assert(map.at(0)[0]->get_value() == 0);

    // Level 1: 2 children (0 + 1, 0 + 2)
    assert(map.at(1).size() == 2);
    assert(map.at(1)[0]->get_value() == 1);
    assert(map.at(1)[1]->get_value() == 2);

    // Level 2: each child has 2 children, total 4
    assert(map.at(2).size() == 4);

    // Level 3: 8 nodes expected
    assert(map.at(3).size() == 8);
}

void test_binary_tree_deep_size() {
    BinaryTree<uint64_t> tree(2);
    size_t size = tree.deep_size();
    assert(size > sizeof(tree)); // Make sure something was counted
}

void test_binary_tree_generate_node_at_valid() {
    Node<uint64_t>* node = BinaryTree<uint64_t>::generate_node_at(3, 4);
    assert(node != nullptr);
    assert(node->get_value() == 13);
    delete node;
    node = BinaryTree<uint64_t>::generate_node_at(5, 12);
    assert(node != nullptr);
    assert(node->get_value() == 57);
    delete node;

    // MPZ Time
    Node<mpz_class>* node_mpz = BinaryTree<mpz_class>::generate_node_at(4, 3);
    assert(node_mpz != nullptr);
    assert(node_mpz->get_value() > 0);
    delete node_mpz;
    node_mpz = BinaryTree<mpz_class>::generate_node_at(5, 12);
    assert(node_mpz != nullptr);
    assert(node_mpz->get_value() == 57);
    delete node_mpz;
}

void test_binary_tree_generate_node_at_invalid_pos() {
    try {
        BinaryTree<uint64_t>::generate_node_at(3, 0);
        assert(false); // Should throw
    } catch (const std::out_of_range& e) {
        assert(std::string(e.what()).find("position 0") != std::string::npos);
    }

    try {
        BinaryTree<uint64_t>::generate_node_at(3, 9); // 2^3 = 8 max
        assert(false); // Should throw
    } catch (const std::out_of_range& e) {
        assert(std::string(e.what()).find("outside of a level") != std::string::npos);
    }
}

void test_binary_tree_with_mpz() {
    BinaryTree<mpz_class> tree(2);
    const auto& root = tree.get_root_node();
    assert(root->get_value() == 0);

    const auto& map = tree.get_level_map();
    assert(map.at(1).size() == 2);
    assert(map.at(2).size() == 4);
}

void test_binary_tree_coverage(size_t levels = 16, size_t threads = 1, const BinaryTreeOptions& opts = BinaryTree<uint64_t>::DEFAULT_OPTS) {
    // Set threads.
    omp_set_num_threads(threads);

    // Unsigned integral form.
    BinaryTree<uint64_t> tree(levels, opts);
    BinaryTreeCoverage<uint64_t> global_coverage;
    size_t target_total = 0;
    for (size_t level=1; level<=tree.get_level_count(); level++) {
        target_total = size_t(1) << level;
        const BinaryTreeCoverage<uint64_t>* coverage = &tree.get_coverage_map().find(level)->second;
        global_coverage.add_covered(coverage->get_covered());
        global_coverage.add_total(coverage->get_total());
        assert(coverage->get_covered() == BinaryTreeCoverageConstants::get_known_coverage(level));
        assert(coverage->get_total() == target_total);
    }
    assert(global_coverage.get_covered() == BinaryTreeCoverageConstants::get_known_coverage_sum_to_level(levels));
    assert(global_coverage.get_total() == BinaryTreeCoverageConstants::get_total_sum_to_level(levels));
    // Tree should always have: 2^(level+1) - 2 nodes total if it's not pruned.
    // We allow pruning of HWM nodes and/or levels.  Need tests for each.
    //
    // Option 1: false and false
    // This is a tree with all its nodes.  Counts should line up without adjustment.
    if (!tree.is_pruning_hwm_nodes() && !tree.is_pruning_parent_levels()) {
        assert(tree.node_count() == (1ULL << (tree.get_level_count() + 1)) - 2);
        assert(tree.node_count_with_root() == (1ULL << (tree.get_level_count() + 1)) - 1);
    }
    // Option 2: true and false
    // HWM pruning is on, but levels are not erased.  Keeps a fulled pared list of non-HWM nodes.  Just add covered back in.
    if (tree.is_pruning_hwm_nodes() && !tree.is_pruning_parent_levels()) {
        size_t node_count_summary = tree.node_count();
        for (size_t level = 1; level <= tree.get_level_count(); level++) {
            node_count_summary += tree.get_coverage_map().find(level)->second.get_covered();
        }
        assert(node_count_summary == BinaryTreeCoverageConstants::get_total_sum_to_level(levels));
    }
    // Option 3: false and true
    // HWM pruning is off, but parent levels are removed.  Keeps a full final level and nothing more.  Add previous known coverage.
    if (!tree.is_pruning_hwm_nodes() && tree.is_pruning_parent_levels()) {
        size_t node_count_summary = tree.node_count() + BinaryTreeCoverageConstants::get_total_sum_to_level(levels - 1);
        assert(node_count_summary == BinaryTreeCoverageConstants::get_total_sum_to_level(levels));
    }
    // Option 4: true and true
    // HWM pruning is on, and parent levels are removed.  Keeps a pared-down final level of non-HWM nodes.
    if (tree.is_pruning_hwm_nodes() && tree.is_pruning_parent_levels()) {
        size_t node_count_plus_uncovered = tree.node_count();
        for (size_t level = 1; level < tree.get_level_count(); level++) {
            node_count_plus_uncovered += tree.get_coverage_map().find(level)->second.get_uncovered();
        }
        assert(node_count_plus_uncovered + BinaryTreeCoverageConstants::get_known_coverage_sum_to_level(levels) == BinaryTreeCoverageConstants::get_total_sum_to_level(levels));
    }

    // GMP (mpz_class) form.
    BinaryTree<mpz_class> tree_mpz(levels, opts);
    BinaryTreeCoverage<mpz_class> global_coverage_mpz;
    mpz_class target_total_mpz = 0;
    for (size_t level=1; level<=tree_mpz.get_level_count(); level++) {
        target_total_mpz = CollatzConstants::MPZ_ONE << level;
        const BinaryTreeCoverage<mpz_class>* coverage_mpz = &tree_mpz.get_coverage_map().find(level)->second;
        global_coverage_mpz.add_covered(coverage_mpz->get_covered());
        global_coverage_mpz.add_total(coverage_mpz->get_total());
        assert(coverage_mpz->get_covered() == BinaryTreeCoverageConstants::get_known_coverage(level));
        assert(coverage_mpz->get_total() == target_total_mpz);
    }
    assert(global_coverage_mpz.get_covered() == BinaryTreeCoverageConstants::get_known_coverage_sum_to_level(levels));
    assert(global_coverage_mpz.get_total() == BinaryTreeCoverageConstants::get_total_sum_to_level(levels));
    // Tree should always have: 2^(level+1) - 2 nodes total if it's not pruned.
    // We allow pruning of HWM nodes and/or levels.  Need tests for each.
    //
    // Option 1: false and false
    // This is a tree with all its nodes.  Counts should line up without adjustment.
    if (!tree_mpz.is_pruning_hwm_nodes() && !tree_mpz.is_pruning_parent_levels()) {
        mpz_class node_count_mpz = 0;
        mpz_ui_pow_ui(node_count_mpz.get_mpz_t(), 2, tree.get_level_count() + 1);
        node_count_mpz -= 2;
        assert(tree_mpz.node_count() == node_count_mpz);
        node_count_mpz += 1;
        assert(tree_mpz.node_count_with_root() == node_count_mpz);
    }
    // Option 2: true and false
    // HWM pruning is on, but levels are not erased.  Keeps a fulled pared list of non-HWM nodes.  Just add covered back in.
    if (tree_mpz.is_pruning_hwm_nodes() && !tree_mpz.is_pruning_parent_levels()) {
        mpz_class node_count_summary = tree_mpz.node_count();
        for (size_t level = 1; level <= tree_mpz.get_level_count(); level++) {
            node_count_summary += tree_mpz.get_coverage_map().find(level)->second.get_covered();
        }
        assert(node_count_summary == BinaryTreeCoverageConstants::get_total_sum_to_level(levels));
    }
    // Option 3: false and true
    // HWM pruning is off, but parent levels are removed.  Keeps a full final level and nothing more.  Add previous known coverage.
    if (!tree_mpz.is_pruning_hwm_nodes() && tree_mpz.is_pruning_parent_levels()) {
        mpz_class node_count_summary = tree_mpz.node_count() + BinaryTreeCoverageConstants::get_total_sum_to_level(levels - 1);
        assert(node_count_summary == BinaryTreeCoverageConstants::get_total_sum_to_level(levels));
    }
    // Option 4: true and true
    // HWM pruning is on, and parent levels are removed.  Keeps a pared-down final level of non-HWM nodes.
    if (tree_mpz.is_pruning_hwm_nodes() && tree_mpz.is_pruning_parent_levels()) {
        mpz_class node_count_plus_uncovered = tree_mpz.node_count();
        for (size_t level = 1; level < tree_mpz.get_level_count(); level++) {
            node_count_plus_uncovered += tree_mpz.get_coverage_map().find(level)->second.get_uncovered();
        }
        assert(node_count_plus_uncovered + BinaryTreeCoverageConstants::get_known_coverage_sum_to_level(levels) == BinaryTreeCoverageConstants::get_total_sum_to_level(levels));
    }
}

void test_binary_tree_node_count_should_match_map() {
    // We do NOT consider root node.  It's a placeholder.
    size_t levels = 4;
    BinaryTree<uint64_t> tree(levels);
    size_t expected_count = (2ULL << levels) - 2;
    size_t map_count = 0;
    const auto& map = tree.get_level_map();
    for (size_t i = 1; i <= tree.get_level_count(); i++) {
        map_count += map.at(i).size();
    }
    assert(tree.node_count() == expected_count);
    assert(map_count == expected_count);
}

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

void test_binary_tree_multi_threaded() {
    size_t max_threads = 4;
    for (size_t threads = 1; threads <= max_threads; threads++) {
        test_binary_tree_coverage(16, threads);
    }
}

template<IntegralOrMPZClass T>
void test_binary_tree_math__type_helper() {
    // Node Levels
    size_t level = 0;  // Always returns size_t
    level = BinaryTreeMath<T>::st_node_level(4);
    assert(level == 2);
    level = BinaryTreeMath<T>::st_node_level(101);
    assert(level == 6);
    level = BinaryTreeMath<T>::st_node_level(1);
    assert(level == 1);
    level = BinaryTreeMath<T>::st_node_level(2);
    assert(level == 1);
    level = BinaryTreeMath<T>::st_node_level(7);
    assert(level == 3);
    level = BinaryTreeMath<T>::st_node_level(14);
    assert(level == 3);
    //
    // Node Positions
    T position = 0;
    position = BinaryTreeMath<T>::st_node_position(1);
    assert(position == 1);
    position = BinaryTreeMath<T>::st_node_position(2);
    assert(position == 2);
    position = BinaryTreeMath<T>::st_node_position(3);
    assert(position == 1);
    position = BinaryTreeMath<T>::st_node_position(4);
    assert(position == 3);
    position = BinaryTreeMath<T>::st_node_position(5);
    assert(position == 2);
    position = BinaryTreeMath<T>::st_node_position(6);
    assert(position == 4);
    position = BinaryTreeMath<T>::st_node_position(46);
    assert(position == 31);
    //
    // Max Node at Level
    mpz_class max_node = 0; // Always returns MPZ
    max_node = BinaryTreeMath<T>::st_max_node_value_at_level(0);
    assert(max_node == 0);
    max_node = BinaryTreeMath<T>::st_max_node_value_at_level(1);
    assert(max_node == 2);
    max_node = BinaryTreeMath<T>::st_max_node_value_at_level(2);
    assert(max_node == 6);
    max_node = BinaryTreeMath<T>::st_max_node_value_at_level(3);
    assert(max_node == 14);
    max_node = BinaryTreeMath<T>::st_max_node_value_at_level(4);
    assert(max_node == 30);
    //
    // Max Full Level at Node
    size_t max_level = 0; // Always returns size_t.
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(1);
    assert(max_level == 0);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(4);
    assert(max_level == 1);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(7);
    assert(max_level == 2);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(26);
    assert(max_level == 3);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(29);
    assert(max_level == 3);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(30);
    assert(max_level == 4);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(49);
    assert(max_level == 4);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(63);
    assert(max_level == 5);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(125);
    assert(max_level == 5);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node(126);
    assert(max_level == 6);
}
void test_binary_tree_math() {
    test_binary_tree_math__type_helper<uint64_t>();
    test_binary_tree_math__type_helper<mpz_class>();

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
    std::cout << "Testing "<< max_test_level << " levels of deep tree build, coverage, and pruning combinations because THIS SHIT CANNOT FAIL!" << std::endl;
    for (size_t level = 1; level <= max_test_level; level++) {
        std::cout << "  Level " << level << "...";
        test_binary_tree_coverage(3, 1, opts_with_hwm_prune_without_level_prune);
        test_binary_tree_coverage(level, 1, opts_with_hwm_prune_with_level_prune);
        test_binary_tree_coverage(level, 1, opts_without_hwm_prune_with_level_prune);
        std::cout << " Okay!" << std::endl;
    }
    // Now test with level pruning too.
    // A pruned tree should always have a drastically smaller size.
    size_t levels = 16;
    float reduction_factor = 0.9;
    BinaryTree<uint64_t> tree_raw(levels);
    BinaryTree<uint64_t> tree_pruned(levels, opts_with_hwm_prune_without_level_prune);
    assert(tree_raw.deep_size() * reduction_factor > tree_pruned.deep_size());
    BinaryTree<mpz_class> tree_raw_mpz(levels);
    BinaryTree<mpz_class> tree_pruned_mpz(levels, opts_with_hwm_prune_without_level_prune);
    assert(tree_raw_mpz.deep_size() * reduction_factor > tree_pruned_mpz.deep_size());
}

int main() {
    test_binary_tree_basic_construction();
    std::cout << "test_binary_tree_basic_construction() passed\n";
    test_binary_tree_deep_size();
    std::cout << "test_binary_tree_deep_size() passed\n";
    test_binary_tree_generate_node_at_valid();
    std::cout << "test_binary_tree_generate_node_at_valid() passed\n";
    test_binary_tree_generate_node_at_invalid_pos();
    std::cout << "test_binary_tree_generate_node_at_invalid_pos() passed\n";
    test_binary_tree_with_mpz();
    std::cout << "test_binary_tree_with_mpz() passed\n";
    test_binary_tree_coverage();
    std::cout << "test_binary_tree_coverage() passed\n";
    test_binary_tree_node_count_should_match_map();
    std::cout << "test_binary_tree_node_count_should_match_map() passed\n";
    test_binary_tree_too_many_levels();
    std::cout << "test_binary_tree_too_many_levels() passed\n";
    test_binary_tree_multi_threaded();
    std::cout << "test_binary_tree_multi_threaded() passed\n";
    test_binary_tree_math();
    std::cout << "test_binary_tree_math() passed\n";
    test_binary_tree_pruned();
    std::cout << "test_binary_tree_pruned() passed\n";


    std::cout << "All BinaryTree<T> tests passed.\n";
    return 0;
}
