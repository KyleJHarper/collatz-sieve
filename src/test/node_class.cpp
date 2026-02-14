#include <cassert>
#include <iostream>
#include <gmpxx.h>
#include "../collatz/node.hpp"



//
// Basic Node Creation
//
template<AnySupportedIntegral T>
void test_basic_node() {
    Node<T> root(7);

    assert(root.get_value() == 7);
    assert(root.get_parent() == nullptr);
    assert(root.get_odd_even_chain_string() == "OEOE");
}



//
// FG Values
//
template<AnySupportedIntegral T>
void test_fg_values() {
    Node<T> node(1);
    assert(node.get_fg_chain_string() == "F"); // Special case
    node.init(2);
    assert(node.get_fg_chain_string() == "G");
    node.init(3);
    assert(node.get_fg_chain_string() == "F");
    node.init(4);
    assert(node.get_fg_chain_string() == "GG");
    node.init(5);
    assert(node.get_fg_chain_string() == "FG");
    node.init(6);
    assert(node.get_fg_chain_string() == "GF");
    node.init(7);
    assert(node.get_fg_chain_string() == "FF");
    node.init(8);
    assert(node.get_fg_chain_string() == "GGG");
    node.init(9);
    assert(node.get_fg_chain_string() == "FGF");
    node.init(10);
    assert(node.get_fg_chain_string() == "GFG");
    node.init(11);
    assert(node.get_fg_chain_string() == "FFG");
    node.init(12);
    assert(node.get_fg_chain_string() == "GGF");
    node.init(13);
    assert(node.get_fg_chain_string() == "FGG");
    node.init(14);
    assert(node.get_fg_chain_string() == "GFF");
}



//
// Resuse via init()
//
template<AnySupportedIntegral T>
void test_reuse_with_init() {
    Node<T> root(7);
    assert(root.get_value() == 7);
    assert(root.get_parent() == nullptr);
    assert(root.get_odd_even_chain_string() == "OEOE");
    root.init(6);
    assert(root.get_value() == 6);
    assert(root.get_odd_even_chain_string() == "EOE");
    assert(root.get_parent() == nullptr);
}



//
// HWM Behavior
//
template<AnySupportedIntegral T>
void test_high_water_mark_behavior() {
    Node<T> root(2);
    Node<T>* child = root.add_child(6);

    assert(child->get_parent() == &root);
    assert(child->has_high_water_mark_ancestor() == true);
    assert(child->get_hwm_ancestor() == &root);
}



//
// HWM Root Case is Special
//
template<AnySupportedIntegral T>
void test_high_water_mark_root_case() {
    Node<T> root(15);
    assert(!root.has_high_water_mark_ancestor());
    assert(root.get_hwm_ancestor() == nullptr);
    assert(!root.is_below_high_water_mark());
}



//
// Children and Deep Size
//
template<AnySupportedIntegral T>
void test_add_child_and_deep_size() {
    Node<T> root(7);
    Node<T>* c1 = root.add_child(3);
    Node<T>* c2 = root.add_child(5);
    assert(c1->get_parent() == &root);
    assert(c2->get_parent() == &root);
    assert(root.does_own_children() == true);
    assert(root.deep_size() > 0);
}



//
// Helper to run all
//
template<AnySupportedIntegral T>
void run_all() {
    std::cout << "test_basic_int_node() ..." << std::flush;
    test_basic_node<T>();
    std::cout << " passed.\n";

    std::cout << "test_fg_values() ..." << std::flush;
    test_fg_values<T>();
    std::cout << " passed.\n";

    std::cout << "test_high_water_mark_behavior() ..." << std::flush;
    test_high_water_mark_behavior<T>();
    std::cout << " passed.\n";

    std::cout << "test_high_water_mark_root_case() ..." << std::flush;
    test_high_water_mark_root_case<T>();
    std::cout << " passed.\n";

    std::cout << "test_add_child_and_deep_size() ..." << std::flush;
    test_add_child_and_deep_size<T>();
    std::cout << " passed.\n";

    std::cout << "test_reuse_with_init() ..." << std::flush;
    test_reuse_with_init<T>();
    std::cout << " passed.\n";
}




int main() {
    std::cout << "=====================" << std::endl;
    std::cout << "Node Tests" << std::endl;
    std::cout << "=====================" << std::endl;

    std::cout << "Performing tests with uint64_t." << std::endl;
    run_all<uint64_t>();

    std::cout << "Performing tests with uint128_t." << std::endl;
    run_all<uint128_t>();

    std::cout << "Performing tests with mpz_class." << std::endl;
    run_all<mpz_class>();

    std::cout << "All Node<T> tests passed.\n";
    return 0;
}
