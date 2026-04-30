#include "collatz/binary_tree.hpp"


int main() {
    ImplicitBinaryTree<uint64_t> tree;
    std::string path = "-";
    tree.load(path);
    std::cout << "I see the tree has " << std::to_string(tree.get_level_count()) << " levels." << std::endl;
    ImplicitBinaryTree<uint64_t> tree2(16);
    assert(tree.equal(tree2));
}
