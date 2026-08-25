#include "collatz/binary_tree.hpp"

int main() {
    BinaryTree<uint64_t> tree(37);
    std::cout << "Size: " << tree.deep_size() << " bytes." << std::endl;
    tree.generate_value_map();
    std::cout << "Size: " << tree.deep_size() << " bytes." << std::endl;
    std::cout << "Value Bitmap Size: " << tree.get_uncovered_values().deep_size() << " bytes." << std::endl;
    std::cout << "Value Bitmap Cardinality: " << tree.get_uncovered_values().cardinality() << " items." << std::endl;
}