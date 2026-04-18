#include "collatz/binary_tree.hpp"



template<typename TreeType>
void do_it(const std::string& name) {
    TreeType tree(8);
    std::cout << "Tree built with " << tree.get_level_count() << " levels." << std::endl;

    std::cout
        << "About to save " + name + " with "
        << tree.node_count()
        << " nodes.  Coverage="
        << to_string_any(tree.get_coverage_map().at(8).get_covered())
        << "/"
        << to_string_any(tree.get_coverage_map().at(8).get_total())
        << " (uncovered: "
        << to_string_any(tree.get_coverage_map().at(8).get_uncovered())
        << ")."
        << std::endl;
    bool success = tree.save(name);
    if (success) {
        std::cout << "Tree saved successfully." << std::endl;
    } else {
        std::cout << "Tree save failed :(" << std::endl;
    }

    TreeType other_tree(8);
    if (tree.equal(other_tree)) {
        std::cout << "match (GOOD)\n";
    } else {
        std::cout << "no match (BAD)\n";
    }

    TreeType different_tree(7);
    if (tree.equal(different_tree)) {
        std::cout << "different trees match (BAD)\n";
    } else {
        std::cout << "different trees don't match (GOOD)\n";
    }
}



int main() {
    do_it<ImplicitBinaryTree<uint64_t>>("implicit_tree.bin");
    do_it<MaterializedBinaryTree<uint64_t>>("material_tree.bin");
}