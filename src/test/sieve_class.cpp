#include <cassert>
#include <cstdint>
#include <iostream>
#include <gmpxx.h>
#include "../collatz/sieve.hpp"


//
// All of these tests need to work with native integrals and GMP.
//




//
// Defaults Regressions
//
template<IntegralOrMPZClass T>
void test_sieve_default_opts() {
    SieveOptions opts;
    assert(opts.pool_size == 4096);
    assert(opts.tree_opts.track_node_metadata == false);
    assert(opts.tree_opts.prune_hwm_nodes == true);
    assert(opts.tree_opts.prune_parent_levels == true);
    assert(opts.tree_opts.preserve_ancestors == true);
}



//
// Pool filling should work under all circumstances.
//
template<IntegralOrMPZClass T>
void test_sieve_pool_filling() {
    SieveOptions opts;
    opts.pool_size = 64;
    Sieve<T> sieve(4, opts);
    assert(sieve.next() == 39);
    assert(sieve.next() == 43);
    assert(sieve.next() == 47);
    assert(sieve.next() == 55);
    assert(sieve.next() == 59);
    assert(sieve.next() == 63);
    assert(sieve.next() == 71);

    // Calling refill without exhasuting the pool shouldn't be an issue.
    assert(sieve.get_pool_index() != 0);
    sieve.refill_pool();
    assert(sieve.get_pool_index() == 0);

    // Values should continue on without missing a beat.
    assert(sieve.next() == 75);
    assert(sieve.next() == 79);
    assert(sieve.next() == 87);
    assert(sieve.next() == 91);
}



//
// Tree source shouldn't matter.
//
template<IntegralOrMPZClass T>
void test_sieve_tree_source() {
    BinaryTree<T> tree(4);
    Sieve<T> sieve(tree);
    Sieve<T> sieve2(4);
    //Compare them
}







//
// Wrapper to run all tests.
//
template<IntegralOrMPZClass T>
void run_all() {
    std::cout << "test_sieve_default_opts ...";
    test_sieve_default_opts<T>();
    std::cout << " passed.\n";

    std::cout << "test_sieve_pool_filling ...";
    test_sieve_pool_filling<T>();
    std::cout << " passed.\n";

    std::cout << "test_sieve_tree_source ...";
    test_sieve_tree_source<T>();
    std::cout << " passed.\n";

}



int main() {
    std::cout << "Performing tests with uint64_t." << std::endl;
    run_all<uint64_t>();
    std::cout << "Performing tests with mpz_class." << std::endl;
    run_all<mpz_class>();

    std::cout << "All Sieve<T> tests passed.\n";
    return 0;
}
