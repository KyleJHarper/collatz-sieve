#include "collatz/binary_tree.hpp"
#include "collatz/concepts.hpp"
#include <filesystem>
#include <stdexcept>






int main(int argc, char** argv) {
    if (argc < 3) {
        throw std::runtime_error("You must specify level count as arg1 and compression level as arg2");
    }
    size_t levels = std::atol(argv[argc-2]);
    size_t compression_level = std::atol(argv[argc-1]);
    std::cerr << levels << std::endl;
    BinaryTreeMath<uint64_t>::set_root_value(1);
    BinaryTreeOptions opts;
    ImplicitBinaryTree<uint64_t> tree(levels, opts);
    // MaterializedBinaryTree<uint64_t> tree(levels, opts);
    std::cerr << "done building first tree\n";
    std::string path = "-";
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
    tree.save(path, compression_level);
    std::cerr << "done saving to disk with compression=" + to_string_any(compression_level) + "\n";


    // std::string path = "whee.bin";
    // if (compression_level > 0) {
    //     path += ".zst";
    // }
    // if (std::filesystem::exists(path)) {
    //     std::filesystem::remove(path);
    // }
    // tree.save(path, compression_level);
    // std::cout << "done saving to disk with compression=" + to_string_any(compression_level) + "\n";
    // ImplicitBinaryTree<uint64_t> tree2;
    // // MaterializedBinaryTree<uint64_t> tree2;
    // tree2.load(path);
    // std::cout << "done loading tree 2\n";
    // assert(tree.equal(tree2));
    // std::cout << "done testing equality\n";
}