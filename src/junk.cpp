#include "collatz/binary_tree.hpp"
#include <filesystem>
#include <stdexcept>






int main(int argc, char** argv) {
    if (argc < 2) {
        throw std::runtime_error("You must specify level count as arg1");
    }
    size_t levels = std::atol(argv[argc-1]);
    std::cout << levels << std::endl;
    BinaryTreeMath<uint64_t>::set_root_value(0);
    BinaryTreeOptions opts;
    // ImplicitBinaryTree<uint64_t> tree(levels, opts);
    MaterializedBinaryTree<uint64_t> tree(levels, opts);
    std::cout << "done building first tree\n";
    std::string path = "whee.bin";
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
    tree.save(path);
    std::cout << "done saving to disk\n";
    // // ImplicitBinaryTree<uint64_t> tree2;
    // MaterializedBinaryTree<uint64_t> tree2;
    // tree2.load(path);
    // std::cout << "done loading tree 2\n";
    // assert(tree.equal(tree2));
    // std::cout << "done testing equality\n";
    // // std::filesystem::remove(path);
}