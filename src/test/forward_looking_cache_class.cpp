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
    ForwardLookingCache<T> flc(1000);
    T val = 42;
    FLCKey<T> key(val);
    flc.contains(key);
    std::cout << flc.get_size() << std::endl;
    assert(flc.contains(key) == false);
    flc.insert(key);
    std::cout << flc.get_size() << std::endl;
    assert(flc.contains(key) == true);
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
