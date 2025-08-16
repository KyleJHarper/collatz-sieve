#include <cassert>
#include <iostream>
#include <gmpxx.h>
#include "../collatz/node.hpp"

void test_basic_node_int() {
    Node<uint64_t> root(7, true);

    assert(root.get_value() == 7);
    assert(root.get_parent() == nullptr);
    assert(root.get_odd_even_chain_string() == "OEOEOE");
    assert(root.get_twos_value() > 0);
    assert(root.get_threes_value() > 0);
    assert(root.get_fg_total() >= 0);
    assert(root.get_fg_n_portion() > 0);
    assert(root.get_fg_constant() >= 0);
}

void test_fg_values() {
    Node<uint64_t> node(1, false);
    assert(node.get_fg_chain_string() == "F");
    node.init(2, false);
    assert(node.get_fg_chain_string() == "G");
    node.init(3, false);
    assert(node.get_fg_chain_string() == "FF");
    node.init(4, false);
    assert(node.get_fg_chain_string() == "GG");
    node.init(5, false);
    assert(node.get_fg_chain_string() == "FG");
    node.init(6, false);
    assert(node.get_fg_chain_string() == "GF");
    node.init(7, false);
    assert(node.get_fg_chain_string() == "FFF");
    node.init(8, false);
    assert(node.get_fg_chain_string() == "GGG");
    node.init(9, false);
    assert(node.get_fg_chain_string() == "FGF");
    node.init(10, false);
    assert(node.get_fg_chain_string() == "GFG");
    node.init(11, false);
    assert(node.get_fg_chain_string() == "FFG");
    node.init(12, false);
    assert(node.get_fg_chain_string() == "GGF");
    node.init(13, false);
    assert(node.get_fg_chain_string() == "FGG");
    node.init(14, false);
    assert(node.get_fg_chain_string() == "GFF");
}


void test_reuse_with_init() {
    Node<uint64_t> root(7, true);
    assert(root.get_value() == 7);
    assert(root.get_parent() == nullptr);
    assert(root.get_odd_even_chain_string() == "OEOEOE");
    assert(root.get_twos_value() > 0);
    assert(root.get_threes_value() > 0);
    assert(root.get_fg_total() >= 0);
    assert(root.get_fg_n_portion() > 0);
    assert(root.get_fg_constant() >= 0);
    root.init(6, true);
    assert(root.get_value() == 6);
    assert(root.get_odd_even_chain_string() == "EOE");
    assert(root.get_parent() == nullptr);
    assert(root.get_twos_value() > 0);
    assert(root.get_threes_value() > 0);
    assert(root.get_fg_total() >= 0);
    assert(root.get_fg_n_portion() > 0);
    assert(root.get_fg_constant() >= 0);
}

void test_high_water_mark_behavior() {
    Node<uint64_t> root(2, true);
    Node<uint64_t>* child = root.add_child(6);

    assert(child->get_parent() == &root);
    assert(child->has_high_water_mark_ancestor() == true);
    assert(child->get_hwm_ancestor() == &root);
}

void test_high_water_mark_root_case() {
    Node<uint64_t> root(15, true);
    assert(!root.has_high_water_mark_ancestor());
    assert(root.get_hwm_ancestor() == nullptr);
    assert(!root.is_below_high_water_mark());
}

void test_add_child_and_deep_size() {
    Node<uint64_t> root(7, false);
    Node<uint64_t>* c1 = root.add_child(3);
    Node<uint64_t>* c2 = root.add_child(5);
    assert(c1->get_parent() == &root);
    assert(c2->get_parent() == &root);
    assert(root.does_own_children() == true);
    assert(root.deep_size() > 0);
}

void test_node_with_mpz() {
    mpz_class val("9");
    Node<mpz_class> node(val, true);
    assert(node.get_value() == val);
    assert(node.get_twos_value() > 0);
    assert(node.get_threes_value() > 0);
    assert(node.get_fg_total() >= 0);
}

void test_metadata_tracking() {
    Node<uint64_t> node(7, true);
    assert(node.get_fg_constant() > 0);  // This should work.
    node.init(6, false);  // This should cause an error when we get metadata.
    try {
        assert(node.get_fg_constant() > 0);
        assert(false); // Should not reach here
    } catch (const std::logic_error& e) {
        assert(std::string(e.what()).find("disabled metadata when") != std::string::npos);
    }
}

int main() {
    test_basic_node_int();
    std::cout << "test_basic_int_node() passed\n";
    test_fg_values();
    std::cout << "test_fg_values() passed\n";
    test_high_water_mark_behavior();
    std::cout << "test_high_water_mark_behavior() passed\n";
    test_high_water_mark_root_case();
    std::cout << "test_high_water_mark_root_case() passed\n";
    test_add_child_and_deep_size();
    std::cout << "test_add_child_and_deep_size() passed\n";
    test_node_with_mpz();
    std::cout << "test_node_with_mpz() passed\n";
    test_metadata_tracking();
    std::cout << "test_metadata_tracking() passed\n";
    test_reuse_with_init();
    std::cout << "test_reuse_with_init() passed\n";

    std::cout << "All Node<T> tests passed.\n";
    return 0;
}
