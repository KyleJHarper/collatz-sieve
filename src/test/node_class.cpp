#include <cassert>
#include <iostream>
#include <gmpxx.h>
#include "../collatz/node.hpp"

void test_basic_node_int() {
    Node<uint> root(7, true);

    assert(root.get_value() == 7);
    assert(root.get_parent() == nullptr);
    assert(!root.get_odd_even_chain().empty());
    assert(root.get_odd_even_chain_string() == "O");
    assert(root.get_twos_value() > 0);
    assert(root.get_threes_value() > 0);
    assert(root.get_fg_total() >= 0);
    assert(root.get_fg_n_portion() > 0);
    assert(root.get_fg_constant() >= 0);
}

void test_reuse_with_init() {
    Node<uint> root(7, true);
    assert(root.get_value() == 7);
    assert(root.get_parent() == nullptr);
    assert(!root.get_odd_even_chain().empty());
    assert(root.get_odd_even_chain_string() == "O");
    assert(root.get_twos_value() > 0);
    assert(root.get_threes_value() > 0);
    assert(root.get_fg_total() >= 0);
    assert(root.get_fg_n_portion() > 0);
    assert(root.get_fg_constant() >= 0);
    root.init(6, true);
    assert(root.get_value() == 6);
    assert(root.get_odd_even_chain_string() == "E");
    assert(root.get_parent() == nullptr);
    assert(!root.get_odd_even_chain().empty());
    assert(root.get_twos_value() > 0);
    assert(root.get_threes_value() > 0);
    assert(root.get_fg_total() >= 0);
    assert(root.get_fg_n_portion() > 0);
    assert(root.get_fg_constant() >= 0);
}

void test_high_water_mark_behavior() {
    Node<uint> root(2, true);
    Node<uint>* child = root.add_child(6);

    assert(child->get_parent() == &root);
    assert(child->has_high_water_mark_ancestor() == true);
    assert(child->get_hwm_ancestor() == &root);
}

void test_high_water_mark_root_case() {
    Node<uint> root(15, true);
    assert(!root.has_high_water_mark_ancestor());
    assert(root.get_hwm_ancestor() == nullptr);
    assert(!root.is_below_high_water_mark());
}

void test_add_child_and_deep_size() {
    Node<uint> root(7, false);
    Node<uint>* c1 = root.add_child(3);
    Node<uint>* c2 = root.add_child(5);
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
    Node<uint> node(7, true);
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
