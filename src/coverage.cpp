#include <cstdio>
#include <gmpxx.h>
#include <stdint.h>
#include <inttypes.h>
#include <iostream>
#include "collatz/binary_tree.hpp"
// #include "experiments/experiment_a1.hpp"


int main() {
    mpz_class value = 1;
    Node bob = Node<mpz_class>(value);
    std::cout << "I am value: " << bob.get_value().get_str() << std::endl;

    uint u_value = 1;
    Node u_bob = Node<uint>(u_value);
    std::cout << "I am u_value: " << u_bob.get_value() << std::endl;

    BinaryTree tree = BinaryTree<uint>(4);
    std::cout << "I see this many levels: " << tree.get_level_count() << std::endl;
    // if (argc < 2) {
    //     std::cerr << "You must send a number as arg1." << std::endl;
    //     return 1;
    // }
    // uint initial_value = atoi(argv[1]);
    // Collatz c = Collatz<uint>(initial_value);
    // std::cout << "Value is: " << c << std::endl;
    // std::cout << c.get_sequence().back() << std::endl;
    // std::cout << "Sequence is: " << c.get_sequence_string() << std::endl;
    // std::cout << "OE pattern is: " << c.get_oe_pattern() << std::endl;
    // std::cout << "HWM index is: " << c.get_hwm_index() << std::endl;
    // std::cout << "Stop count: " << c.get_stop_count() << std::endl;

    return 0;
}
