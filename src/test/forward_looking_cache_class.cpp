#include <cassert>
#include <cstdint>
#include <iostream>
#include <gmpxx.h>
#include "../collatz/concepts.hpp"
#include "../collatz/forward_looking_cache.hpp"



//
// All of these tests need to work with native integrals and GMP.
//



//
// Defaults Regressions
//
template<IntegralOrMPZClass T>
void test_flc_default_opts() {
    assert(FLCKeySize == 24);
}



//
// Basic Usage
//
template<IntegralOrMPZClass T>
void test_flc_basic() {
    size_t size = 10;
    ForwardLookingCache<T> flc(size);
    T val = 42;
    FLCKey<T> key(val);
    // Miss
    assert(flc.get_misses() == 0);
    assert(flc.contains(key) == false);
    assert(flc.get_misses() == 1);
    // Insert
    assert(flc.get_new_insertions() == 0);
    flc.insert(key);
    assert(flc.get_new_insertions() == 1);
    // Hit
    assert(flc.get_hits() == 0);
    assert(flc.contains(key) == true);
    assert(flc.get_hits() == 1);
    // Overlap
    assert(flc.get_overlap_insertions() == 0);
    flc.insert(key);
    assert(flc.get_overlap_insertions() == 1);
    // Evictions
    assert(flc.get_evictions() == 0);
    for (T value = 1; value <= size; value += 1) {
        key.serialize(value);
        flc.insert(key);
    }
    assert(flc.get_evictions() == 1);
}



//
// LRU Order
//
template<IntegralOrMPZClass T>
void test_flc_lru() {
    size_t size = 10;
    ForwardLookingCache<T> flc(size);
    T val = 42;
    FLCKey<T> key(val);

}



//
// Wrapper to run all tests.
//
template<IntegralOrMPZClass T>
void run_all() {
    std::cout << "test_flc_default_opts ..." << std::flush;
    test_flc_default_opts<T>();
    std::cout << " passed.\n";

    std::cout << "test_flc_basic ..." << std::flush;
    test_flc_basic<T>();
    std::cout << " passed.\n";
}



int main() {
    std::cout << "Performing tests with uint64_t." << std::endl;
    run_all<uint64_t>();
    std::cout << "Performing tests with mpz_class." << std::endl;
    run_all<mpz_class>();

    std::cout << "All ForwardLookingCache<T> tests passed.\n";
    return 0;
}
