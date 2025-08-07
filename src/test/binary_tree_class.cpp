#include <cassert>
#include <iostream>
#include <gmpxx.h>
#include "../collatz/binary_tree.hpp" // Include your BinaryTree and Node classes

void test_binary_tree_basic_construction() {
    BinaryTree<uint> tree(3);
    // Build extras to ensure isolation, double-freeing, etc are all good.  These will destruct at function end (end of scope).
    BinaryTree<uint> tree2(2);
    BinaryTree<uint> tree3(4);
    BinaryTree<uint> tree4(5);
    BinaryTree<uint> tree5(6);
    BinaryTree<uint> tree6(7);
    assert(tree.get_max_level() == 3);
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
    BinaryTree<uint> tree(2);
    size_t size = tree.deep_size();
    assert(size > sizeof(tree)); // Make sure something was counted
}

void test_binary_tree_generate_node_at_valid() {
    Node<uint>* node = BinaryTree<uint>::generate_node_at(3, 4);
    assert(node != nullptr);
    assert(node->get_value() > 0);
    delete node;
}

void test_binary_tree_generate_node_at_invalid_pos() {
    try {
        BinaryTree<uint>::generate_node_at(3, 0);
        assert(false); // Should throw
    } catch (const std::runtime_error& e) {
        assert(std::string(e.what()).find("position 0") != std::string::npos);
    }

    try {
        BinaryTree<uint>::generate_node_at(3, 9); // 2^3 = 8 max
        assert(false); // Should throw
    } catch (const std::runtime_error& e) {
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

void test_generate_node_at_mpz() {
    Node<mpz_class>* node = BinaryTree<mpz_class>::generate_node_at(4, 3);
    assert(node != nullptr);
    assert(node->get_value() > 0);
    delete node;
}

void test_binary_tree_16_levels() {
    BinaryTree<uint> tree(16);
    BinaryTreeCoverage global_coverage;
    size_t target_covered = 0;
    size_t target_total = 0;
    for (size_t level=1; level<=tree.get_max_level(); level++) {
        target_total = std::pow(2, level);
        const BinaryTreeCoverage* coverage = &tree.get_coverage_map().find(level)->second;
        global_coverage.add_covered(coverage->get_covered());
        global_coverage.add_total(coverage->get_total());
        switch (level) {
            case 1:
                target_covered = 1;
                break;
            case 2:
                target_covered = 3;
                break;
            case 3:
                target_covered = 6;
                break;
            case 4:
                target_covered = 13;
                break;
            case 5:
                target_covered = 28;
                break;
            case 6:
                target_covered = 56;
                break;
            case 7:
                target_covered = 115;
                break;
            case 8:
                target_covered = 237;
                break;
            case 9:
                target_covered = 474;
                break;
            case 10:
                target_covered = 960;
                break;
            case 11:
                target_covered = 1920;
                break;
            case 12:
                target_covered = 3870;
                break;
            case 13:
                target_covered = 7825;
                break;
            case 14:
                target_covered = 15650;
                break;
            case 15:
                target_covered = 31473;
                break;
            case 16:
                target_covered = 63422;
                break;
            default:
                assert(false);
                break;
        }
        assert(coverage->get_covered() == target_covered);
        assert(coverage->get_total() == target_total);
    }
    assert(global_coverage.get_covered() == 126053);
    assert(global_coverage.get_total() == 131070);
    // Tree should always have: 2^(level+1) - 2 nodes total.
    assert(tree.node_count() == (std::pow(2, tree.get_max_level() + 1) - 2));
    assert(tree.node_count_with_root() == (std::pow(2, tree.get_max_level() + 1) - 1));
    BinaryTree<mpz_class> tree_mpz(16);
    BinaryTreeCoverage global_coverage_mpz;
    size_t target_covered_mpz = 0;
    size_t target_total_mpz = 0;
    for (size_t level=1; level<=tree_mpz.get_max_level(); level++) {
        target_total_mpz = std::pow(2, level);
        const BinaryTreeCoverage* coverage_mpz = &tree_mpz.get_coverage_map().find(level)->second;
        global_coverage_mpz.add_covered(coverage_mpz->get_covered());
        global_coverage_mpz.add_total(coverage_mpz->get_total());
        switch (level) {
            case 1:
                target_covered_mpz = 1;
                break;
            case 2:
                target_covered_mpz = 3;
                break;
            case 3:
                target_covered_mpz = 6;
                break;
            case 4:
                target_covered_mpz = 13;
                break;
            case 5:
                target_covered_mpz = 28;
                break;
            case 6:
                target_covered_mpz = 56;
                break;
            case 7:
                target_covered_mpz = 115;
                break;
            case 8:
                target_covered_mpz = 237;
                break;
            case 9:
                target_covered_mpz = 474;
                break;
            case 10:
                target_covered_mpz = 960;
                break;
            case 11:
                target_covered_mpz = 1920;
                break;
            case 12:
                target_covered_mpz = 3870;
                break;
            case 13:
                target_covered_mpz = 7825;
                break;
            case 14:
                target_covered_mpz = 15650;
                break;
            case 15:
                target_covered_mpz = 31473;
                break;
            case 16:
                target_covered_mpz = 63422;
                break;
            default:
                assert(false);
                break;
        }
        assert(coverage_mpz->get_covered() == target_covered_mpz);
        assert(coverage_mpz->get_total() == target_total_mpz);
    }
    assert(global_coverage_mpz.get_covered() == 126053);
    assert(global_coverage_mpz.get_total() == 131070);
    // Tree should always have: 2^(level+1) - 2 nodes total.
    assert(tree_mpz.node_count() == (std::pow(2, tree_mpz.get_max_level() + 1) - 2));
    assert(tree_mpz.node_count_with_root() == (std::pow(2, tree_mpz.get_max_level() + 1) - 1));
}

void test_binary_tree_node_count_should_match_map() {
    // We do NOT consider root node.  It's a placeholder.
    size_t levels = 4;
    BinaryTree<uint64_t> tree(levels);
    size_t expected_count = (2ULL << levels) - 2;
    size_t map_count = 0;
    const auto& map = tree.get_level_map();
    for (size_t i = 1; i <= tree.get_max_level(); i++) {
        map_count += map.at(i).size();
    }
    assert(tree.node_count() == expected_count);
    assert(map_count == expected_count);
}

int main() {
    Node<uint>::enable_sequenes();  // Ensure Collatz data is kept

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
    test_generate_node_at_mpz();
    std::cout << "test_generate_node_at_mpz() passed\n";
    test_binary_tree_16_levels();
    std::cout << "test_binary_tree_16_levels() passed\n";
    test_binary_tree_node_count_should_match_map();
    std::cout << "test_binary_tree_node_count_should_match_map() passed\n";

    std::cout << "All BinaryTree<T> tests passed.\n";
    return 0;
}
