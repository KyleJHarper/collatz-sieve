#include <cassert>
#include <iostream>
#include <gmpxx.h>
#include "../collatz/node.hpp"

void test_basic_node_int() {
    Node<int>::enable_sequenes();
    Node<int> root(7);

    assert(root.get_value() == 7);
    assert(root.get_parent() == nullptr);
    assert(!root.get_odd_even_chain().empty());
    assert(root.get_twos_value() > 0);
    assert(root.get_threes_value() > 0);
    assert(root.get_fg_total() >= 0);
    assert(root.get_fg_n_portion() > 0);
    assert(root.get_fg_constant() >= 0);
    assert(!root.get_collatz()->get_sequence().empty());
}

void test_high_water_mark_behavior() {
    Node<int>::enable_sequenes();
    Node<int> root(6);
    Node<int>* child = root.add_child(5);

    assert(child->get_parent() == &root);
    assert(child->has_high_water_mark_ancestor()); // child is below HWM
    assert(child->get_hwm_ancestor() == &root);
}

void test_high_water_mark_root_case() {
    Node<int>::enable_sequenes();
    Node<int> root(15);
    assert(!root.has_high_water_mark_ancestor());
    assert(root.get_hwm_ancestor() == nullptr);
    assert(!root.is_below_high_water_mark());
}

void test_add_child_and_deep_size() {
    Node<int>::enable_sequenes();
    Node<int> root(7);
    Node<int>* c1 = root.add_child(3);
    Node<int>* c2 = root.add_child(5);
    assert(c1->get_parent() == &root);
    assert(c2->get_parent() == &root);
    assert(root.deep_size() > 0);
}

void test_node_with_mpz() {
    mpz_class val("9");
    Node<mpz_class>::enable_sequenes();
    Node<mpz_class> node(val);

    assert(node.get_value() == val);
    assert(node.get_twos_value() > 0);
    assert(node.get_threes_value() > 0);
    assert(node.get_fg_total() >= 0);
}

void test_keep_sequences_toggle() {
    Node<int>::disable_sequenes();
    Node<int> node(6);
    // Internal _collatz sequence should be cleared
    try {
        assert(node.get_collatz()->get_sequence().empty() == true);
        assert(false); // Should not reach here
    } catch (const std::logic_error& e) {
        assert(std::string(e.what()).find("disabled sequence tracking") != std::string::npos);
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
    test_keep_sequences_toggle();
    std::cout << "test_keep_sequences_toggle() passed\n";

    std::cout << "All Node<T> tests passed.\n";
    return 0;
}
