#include <iostream>
// #include "absl/numeric/int128.h"
#include "collatz/binary_tree.hpp"

int main() {
    // ImplicitBinaryTree<absl::uint128> tree(16);
    ImplicitBinaryTree<uint128_t> tree(16);
    std::cout << "I made a tree, using absl" << std::endl;
}
