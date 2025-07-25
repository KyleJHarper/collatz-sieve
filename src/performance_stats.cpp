#include <cstdio>
#include <gmpxx.h>
#include <stdint.h>
#include <inttypes.h>
#include <iostream>
#include "collatz/binary_tree.hpp"
#include "collatz/collatz.hpp"
// #include "experiments/experiment_a1.hpp"


int main() {
    // Size Data
    std::cout << "Size of a mpz_class is: " << sizeof(mpz_class) << std::endl;
    std::cout << "Size of a Collatz<uint> is: " << sizeof(Collatz<uint>) << std::endl;
    std::cout << "Size of a Collatz<mpz_class> is: " << sizeof(Collatz<mpz_class>) << std::endl;
    std::cout << "Size of a Node<uint> is: " << sizeof(Node<uint>) << std::endl;
    std::cout << "Size of a Node<mpz_class> is: " << sizeof(Node<mpz_class>) << std::endl;
    std::cout << "Size of a BinaryTree<uint> is: " << sizeof(BinaryTree<uint>) << std::endl;
    std::cout << "Size of a BinaryTree<mpz_class> is: " << sizeof(BinaryTree<mpz_class>) << std::endl;
    return 0;
}
