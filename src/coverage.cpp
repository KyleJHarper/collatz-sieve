#include <cstdio>
#include <gmpxx.h>
#include <stdint.h>
#include <inttypes.h>
#include <iostream>
#include <unordered_map>
#include "collatz/binary_tree.hpp"
// #include "experiments/experiment_a1.hpp"


int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "You must send a number as arg1 for the number of levels." << std::endl;
        return 1;
    }

    // Build the tree.
    typedef mpz_class my_type;
    size_t levels = 0;
    levels = atoi(argv[1]);
    BinaryTree tree = BinaryTree<my_type>(levels);

    // Calculate coverage.
    std::unordered_map<size_t, my_type> coverage_map;
    for(size_t level=1; level <= tree.get_max_level(); level++) {
        const std::vector<Node<my_type>*>& nodes = tree.get_level_map().find(level)->second;
        for(const Node<my_type>* node : nodes) {
            if(node->is_below_high_water_mark() || node->has_high_water_mark_ancestor()) {
                coverage_map[level] += 1;
            }
        }
    }

    // Print results.
    my_type global_covered = 0;
    my_type global_total = 0;
    my_type total = 0;
    my_type covered = 0;
    mpf_class coverage = 0;
    for(size_t level=1; level <= tree.get_max_level(); level++) {
        const std::vector<Node<my_type>*>& nodes = tree.get_level_map().find(level)->second;
        covered = coverage_map[level];
        total = nodes.size();
        global_total += total;
        global_covered += covered;
        // Integer division will make you sad.  Get a float!
        coverage = (mpf_class(covered) / total) * 100;
        std::cout << "Level " << level << ": " << coverage << "%  (" << covered << "/" << total << ")" << std::endl;
    }
    // Global Print
    coverage = (mpf_class(global_covered) / global_total) * 100;
    std::cout << "Global Coverage: " << coverage << "%  (" << global_covered << "/" << global_total << ")" << std::endl;

    return 0;
}
