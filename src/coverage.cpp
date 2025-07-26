#include <gmpxx.h>
#include <iostream>
#include <stdint.h>
#include "collatz/binary_tree.hpp"


int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "You must send a number as arg1 for the number of levels." << std::endl;
        return 1;
    }

    // Build the tree.
    typedef mpz_class my_type;
    // typedef uint64_t my_type;
    size_t levels = 0;
    levels = atoi(argv[1]);
    BinaryTree tree = BinaryTree<my_type>(levels);

    BinaryTreeCoverage global_coverage;
    for (size_t level=1; level<=tree.get_max_level(); level++) {
        BinaryTreeCoverage coverage = tree.get_coverage_map().find(level)->second;
        std::cout << "Level " << level << ": " << coverage.get_ratio(true) << "%  (" << coverage.get_covered() << "/" << coverage.get_total() << ")" << std::endl;
        global_coverage.add_covered(coverage.get_covered());
        global_coverage.add_total(coverage.get_total());
    }
    std::cout << "Global Coverage: " << global_coverage.get_ratio(true) << "%  (" << global_coverage.get_covered() << "/" << global_coverage.get_total() << ")" << std::endl;

    return 0;
}
