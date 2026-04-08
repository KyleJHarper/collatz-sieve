#include "collatz/binary_tree.hpp"


int main() {
    BinaryTreeOptions opts;
    opts.tree_type = BinaryTreeType::IMPLICIT;
    BinaryTree<uint64_t> tree(3);
    std::cout << "Tree built with " << tree.get_level_count() << " levels." << std::endl;
}