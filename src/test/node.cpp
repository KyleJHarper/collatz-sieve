#include <cassert>
#include <cstdint>
#include <limits>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"
#include "../collatz/node.hpp"



template<AnySupportedIntegral T>
void test_node_basics() {
    start_test(__func__);

    // Default initialize is okay.
    Node<T> node;
    assert(node.get_value() == 0);
    assert(node.get_parent() == nullptr);
    assert(node.get_ast_ancestor() == nullptr);
    assert(node.get_child(0) == nullptr);
    assert(node.get_child(1) == nullptr);
    assert(node.is_below_ast() == false);
    assert(node.has_ast_ancestor() == false);
    assert(node.is_initialized() == false);
    assert(node.does_own_children() == true);
    assert(node.get_child_count() == 0);

    // Pass a value.
    Node<T> node2(4);
    assert(node2.get_value() == 4);
    assert(node2.get_parent() == nullptr);
    assert(node2.get_ast_ancestor() == nullptr);
    assert(node2.get_child(0) == nullptr);
    assert(node2.get_child(1) == nullptr);
    assert(node2.is_below_ast() == true);
    assert(node2.has_ast_ancestor() == false);
    assert(node2.is_initialized() == true);
    assert(node2.does_own_children() == true);
    assert(node2.get_child_count() == 0);

    // Pass a value with a parent
    Node<T> node3(8, &node2);
    assert(node3.get_value() == 8);
    assert(node3.get_parent() == &node2);
    assert(node3.get_ast_ancestor() == &node2);
    assert(node3.get_child(0) == nullptr);
    assert(node3.get_child(1) == nullptr);
    assert(node3.is_below_ast() == true);
    assert(node3.has_ast_ancestor() == true);
    assert(node3.is_initialized() == true);
    assert(node3.does_own_children() == true);
    assert(node3.get_child_count() == 0);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_init() {
    start_test(__func__);

    Node<T> node;
    assert(node.get_value() == 0);
    assert(node.get_parent() == nullptr);
    assert(node.get_ast_ancestor() == nullptr);
    assert(node.get_child(0) == nullptr);
    assert(node.get_child(1) == nullptr);
    assert(node.is_below_ast() == false);
    assert(node.has_ast_ancestor() == false);
    assert(node.is_initialized() == false);
    assert(node.does_own_children() == true);
    assert(node.get_child_count() == 0);

    // Now init.
    node.init(4);
    assert(node.get_value() == 4);
    assert(node.get_parent() == nullptr);
    assert(node.get_ast_ancestor() == nullptr);
    assert(node.get_child(0) == nullptr);
    assert(node.get_child(1) == nullptr);
    assert(node.is_below_ast() == true);
    assert(node.has_ast_ancestor() == false);
    assert(node.is_initialized() == true);
    assert(node.does_own_children() == true);
    assert(node.get_child_count() == 0);

    // Init again.
    node.init(8);
    assert(node.get_value() == 8);
    assert(node.get_parent() == nullptr);
    assert(node.get_ast_ancestor() == nullptr);
    assert(node.get_child(0) == nullptr);
    assert(node.get_child(1) == nullptr);
    assert(node.is_below_ast() == true);
    assert(node.has_ast_ancestor() == false);
    assert(node.is_initialized() == true);
    assert(node.does_own_children() == true);
    assert(node.get_child_count() == 0);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_reset() {
    start_test(__func__);

    Node<T> node(4);
    assert(node.get_value() == 4);
    assert(node.get_parent() == nullptr);
    assert(node.get_ast_ancestor() == nullptr);
    assert(node.get_child(0) == nullptr);
    assert(node.get_child(1) == nullptr);
    assert(node.is_below_ast() == true);
    assert(node.has_ast_ancestor() == false);
    assert(node.is_initialized() == true);
    assert(node.does_own_children() == true);
    assert(node.get_child_count() == 0);

    // Reset
    node.reset();
    assert(node.get_value() == 0);
    assert(node.get_parent() == nullptr);
    assert(node.get_ast_ancestor() == nullptr);
    assert(node.get_child(0) == nullptr);
    assert(node.get_child(1) == nullptr);
    assert(node.is_below_ast() == false);
    assert(node.has_ast_ancestor() == false);
    assert(node.is_initialized() == false);
    assert(node.does_own_children() == true);
    assert(node.get_child_count() == 0);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_add_child() {
    start_test(__func__);

    Node<T> node(1);
    assert(node.get_child_count() == 0);
    assert(node.get_child(0) == nullptr);
    assert(node.get_child(1) == nullptr);
    Node<T>* child = node.add_child(2);
    assert(node.get_child_count() == 1);
    assert(node.get_child(0) == child);
    assert(node.get_child(1) == nullptr);

    // Child will be cleaned up when ~node() fires.

    end_test();
}



template<AnySupportedIntegral T>
void test_node_assign_child() {
    start_test(__func__);

    Node<T> node(1);
    Node<T>* child = new Node<T>(2);
    assert(node.get_child_count() == 0);
    assert(node.get_child(0) == nullptr);
    assert(node.get_child(1) == nullptr);
    node.assign_child(child);
    assert(node.get_child_count() == 1);
    assert(node.get_child(0) == child);
    assert(node.get_child(1) == nullptr);

    // Child will be cleaned up when ~node() fires.

    end_test();
}



template<AnySupportedIntegral T>
void test_node_own_children() {
    start_test(__func__);

    Node<T> node(1);
    assert(node.does_own_children() == true);
    node.own_children(false);
    assert(node.does_own_children() == false);
    node.own_children(true);
    assert(node.does_own_children() == true);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_release_child() {
    start_test(__func__);

    Node<T> node(1);
    Node<T>* child = new Node<T>(2);
    Node<T>* someone_elses_kiddo = new Node<T>(5);
    assert(node.get_child_count() == 0);
    assert(node.get_child(0) == nullptr);
    assert(node.get_child(1) == nullptr);

    // Assign child
    node.assign_child(child);
    assert(node.get_child_count() == 1);
    assert(node.get_child(0) == child);
    assert(node.get_child(1) == nullptr);

    // Remove a child not owned by node.
    node.release_child(someone_elses_kiddo);
    assert(node.get_child_count() == 1);
    assert(node.get_child(0) == child);
    assert(node.get_child(1) == nullptr);

    // Remove a child owned by node.
    node.release_child(child);
    assert(node.get_child_count() == 0);
    assert(node.get_child(0) == nullptr);
    assert(node.get_child(1) == nullptr);

    // Clean up.  Node cleaned up child already.
    delete someone_elses_kiddo;

    end_test();
}



template<AnySupportedIntegral T>
void test_node_release_children() {
    start_test(__func__);

    Node<T> node(1);
    Node<T>* child_1 = new Node<T>(2);
    Node<T>* child_2 = new Node<T>(3);
    assert(node.get_child_count() == 0);
    assert(node.get_child(0) == nullptr);
    assert(node.get_child(1) == nullptr);

    // Assign children
    node.assign_child(child_1);
    assert(node.get_child_count() == 1);
    assert(node.get_child(0) == child_1);
    assert(node.get_child(1) == nullptr);
    node.assign_child(child_2);
    assert(node.get_child_count() == 2);
    assert(node.get_child(0) == child_1);
    assert(node.get_child(1) == child_2);

    // Release
    node.release_children();
    assert(node.get_child_count() == 0);
    assert(node.get_child(0) == nullptr);
    assert(node.get_child(1) == nullptr);

    // Clean up.  Calling release_children() deleted children.

    end_test();
}



template<AnySupportedIntegral T>
void test_node_does_own_children() {
    start_test(__func__);

    Node<T> node(1);
    assert(node.does_own_children() == true);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_get_child() {
    start_test(__func__);

    Node<T> node(1);
    node.add_child(2);
    const Node<T>* child = node.get_child(0);
    assert(child != nullptr);
    assert(child->get_value() == 2);

    // Unsafe version
    Node<T>* child_unsafe = node.get_child_unsafe(0);
    assert(child_unsafe != nullptr);
    assert(child_unsafe->get_value() == 2);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_get_child_count() {
    start_test(__func__);

    Node<T> node(1);
    Node<T>* child_1 = new Node<T>(2);
    Node<T>* child_2 = new Node<T>(3);
    assert(node.get_child_count() == 0);
    node.assign_child(child_1);
    assert(node.get_child_count() == 1);
    node.assign_child(child_2);
    assert(node.get_child_count() == 2);
    node.release_child(child_1);
    assert(node.get_child_count() == 1);
    node.release_child(child_2);
    assert(node.get_child_count() == 0);

    // The release() free'd memory for children.

    end_test();
}



template<AnySupportedIntegral T>
void test_node_assign_parent() {
    start_test(__func__);

    Node<T> parent(1);
    Node<T> node(2);
    assert(node.get_parent() == nullptr);
    node.assign_parent(&parent);
    assert(node.get_parent() == &parent);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_get_parent() {
    start_test(__func__);

    // Manual assignment.
    Node<T> parent(1);
    Node<T> node(2);
    assert(node.get_parent() == nullptr);
    node.assign_parent(&parent);
    assert(node.get_parent() == &parent);

    // Automatic assignment.
    Node<T> parent_1(1);
    Node<T> node_1(2, &parent_1);
    assert(node_1.get_parent() == &parent_1);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_assign_ast_ancestor() {
    start_test(__func__);

    Node<T> ast_ancestor(2);
    Node<T> node(4);
    assert(node.get_ast_ancestor() == nullptr);
    node.assign_ast_ancestor(&ast_ancestor);
    assert(node.get_ast_ancestor() == &ast_ancestor);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_get_ast_ancestor() {
    start_test(__func__);

    // Manual assignment (only option).
    Node<T> ast_ancestor(2);
    Node<T> node(4);
    assert(node.get_ast_ancestor() == nullptr);
    node.assign_ast_ancestor(&ast_ancestor);
    assert(node.get_ast_ancestor() == &ast_ancestor);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_st_get_ast_index() {
    start_test(__func__);

    Node<T> node;
    for(T i = 1; i < 100000; i++) {
        T current = i;
        seq_size_t step = 0;
        T manual_ast_index = 0;
        while (current > 1) {
            if (current % 2 == 1) {
                current = (3 * current) + 1;
            } else {
                current = current / 2;
            }
            step++;
            if (current < i) {
                manual_ast_index = step;
                break;
            }
        }
        node.init(i);
        assert(manual_ast_index == Node<T>::st_get_ast_index(i));
        assert(manual_ast_index == node.get_ast_index());
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_node_get_fg_chain_length() {
    start_test(__func__);

    // Uses BinaryTreeMath under the hood.
    // If BinaryTreeMath passes all tests, this is safe to simplify.
    Node<T> node;
    for (T i = 1; i < 1000000; i++) {
        node.init(i);
        level_t level = BinaryTreeMath<T>::st_get_level_by_node_value(i);
        assert(node.get_fg_chain_length() == BinaryTreeMath<T>::st_fg_chain_length(level));
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_node_get_level() {
    start_test(__func__);

    // Uses BinaryTreeMath under the hood.
    // If BinaryTreeMath passes all tests, this is safe to simplify.
    Node<T> node;
    for (T i = 1; i < 1000000; i++) {
        node.init(i);
        assert(node.get_level() == BinaryTreeMath<T>::st_get_level_by_node_value(i));
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_node_get_position() {
    start_test(__func__);

    // Uses BinaryTreeMath under the hood.
    // If BinaryTreeMath passes all tests, this is safe to simplify.
    Node<T> node;
    for (T i = 1; i < 1000000; i++) {
        node.init(i);
        assert(node.get_position() == BinaryTreeMath<T>::st_node_position(i));
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_node_get_fg_chain_string() {
    start_test(__func__);

    Node<T> node;

    // Manually test a few.
    node.init(1);
    assert(node.get_fg_chain_string() == "");
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

    // Programmatically verify a lot more.  Start at 2 because 1 == "", which is verified above.
    for (T value = 2; value < 100000; value++) {
        node.init(value);
        seq_size_t max_chars = node.get_fg_chain_length();
        std::string manual_chain = "";
        Collatz<T>::st_for_each_fg_chain_link(value, [&](bool is_F) {
            manual_chain += (is_F ? 'F' : 'G');
            if (manual_chain.size() >= max_chars) {
                return ForEachSignal::BREAK;
            }
            return ForEachSignal::CONTINUE;
        });
        assert(manual_chain == node.get_fg_chain_string());
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_node_get_odd_even_chain_string() {
    start_test(__func__);

    Node<T> node;

    // Manually test a few.
    node.init(1);
    assert(node.get_odd_even_chain_string() == "");
    node.init(2);
    assert(node.get_odd_even_chain_string() == "E");
    node.init(3);
    assert(node.get_odd_even_chain_string() == "OE");
    node.init(4);
    assert(node.get_odd_even_chain_string() == "EE");
    node.init(5);
    assert(node.get_odd_even_chain_string() == "OEE");
    node.init(6);
    assert(node.get_odd_even_chain_string() == "EOE");
    node.init(7);
    assert(node.get_odd_even_chain_string() == "OEOE");
    node.init(8);
    assert(node.get_odd_even_chain_string() == "EEE");
    node.init(9);
    assert(node.get_odd_even_chain_string() == "OEEOE");
    node.init(10);
    assert(node.get_odd_even_chain_string() == "EOEE");
    node.init(11);
    assert(node.get_odd_even_chain_string() == "OEOEE");
    node.init(12);
    assert(node.get_odd_even_chain_string() == "EEOE");
    node.init(13);
    assert(node.get_odd_even_chain_string() == "OEEE");
    node.init(14);
    assert(node.get_odd_even_chain_string() == "EOEOE");

    // Programmatically verify a lot more.  Start at 2 because 1 == "", which is verified above.
    for (T value = 2; value < 100000; value++) {
        node.init(value);
        seq_size_t max_chars = node.get_fg_chain_length();
        std::string manual_fg_chain = "";
        std::string manual_oe_chain = "";
        Collatz<T>::st_for_each_fg_chain_link(value, [&](bool is_F) {
            manual_fg_chain += (is_F ? 'F' : 'G');
            if (manual_fg_chain.size() >= max_chars) {
                return ForEachSignal::BREAK;
            }
            return ForEachSignal::CONTINUE;
        });
        // Convert.  Don't strip the last E because these are partial sequences not ending in 1.
        manual_oe_chain = Collatz<T>::st_fg_to_oe(manual_fg_chain, std::numeric_limits<size_t>::max(), false);
        assert(manual_oe_chain == node.get_odd_even_chain_string());
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_node_deep_size() {
    start_test(__func__);

    Node<T> node(1);
    assert(node.deep_size() > 1);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_cloneable() {
    start_test(__func__);

    // Node uses a different signature because of parent/ast values.  Can't use cloneable() helper.
    Node<T> node(3);

    // Make a stream and error string.
    std::stringstream stream;
    std::string err;

    // Serialize to it.
    if (! node.serialize(stream, &err)) {
        std::cout << "Failed to seralize for cloning.  Error is: " << err << std::endl;
        assert(false);
    }

    // Make a blank object.
    Node<T> copy;
    T parent_v = 0;
    T ast_ancestor_v = 0;
    uint8_t child_count = 0;

    // Deserialize into it.
    if (! copy.deserialize(stream, parent_v, ast_ancestor_v, child_count, &err)) {
        std::cout << "Failed to deseralize for cloning.  Error is: " << err << std::endl;
        assert(false);
    }

    // Confirm equality.
    if (! copy.equal(node, &err)) {
        std::cout << "Cloned object is not equal to source.  Error is: " << err << std::endl;
        assert(false);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_node_equality() {
    start_test(__func__);

    Node<T> first(3);
    Node<T> second(3);
    Node<T> different(4);
    assert(equality(first, second, different));

    end_test();
}



template<AnySupportedIntegral T>
void test_node_() {
    start_test(__func__);


    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_node_basics<T>();
    test_node_init<T>();
    test_node_reset<T>();
    test_node_add_child<T>();
    test_node_assign_child<T>();
    test_node_own_children<T>();
    test_node_release_child<T>();
    test_node_release_children<T>();
    test_node_does_own_children<T>();
    test_node_get_child<T>();
    test_node_get_child_count<T>();
    test_node_assign_parent<T>();
    test_node_get_parent<T>();
    test_node_assign_ast_ancestor<T>();
    test_node_get_ast_ancestor<T>();
    test_node_st_get_ast_index<T>();
    test_node_get_fg_chain_length<T>();
    test_node_get_level<T>();
    test_node_get_position<T>();
    test_node_get_fg_chain_string<T>();
    test_node_get_odd_even_chain_string<T>();
    test_node_deep_size<T>();
    test_node_cloneable<T>();
    test_node_equality<T>();
}



int main() {
    std::string name = "Node";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
