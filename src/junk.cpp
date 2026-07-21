#include "collatz/binary_tree.hpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Must pass level count as arg1." << std::endl;
        return 1;
    }
    level_t level = atoll(argv[1]);
    std::cout << "I see level=" << level << std::endl;
    std::string filename = "tree_i_" + std::to_string(level) + "_ivm.zstd";

    std::cout << "Building tree\n";
    ImplicitBinaryTree<uint64_t> tree(level);
    std::cout << "Generating value map\n";
    tree.generate_value_map();
    std::cout << "I see cardinality=" << to_string_any(tree.get_uncovered_values().cardinality()) << std::endl;
    std::cout << "Saving\n";
    tree.save(filename, 16);
    // i == implicit
    // 40 == level 40 tree
    // ivm == includes value map
}
