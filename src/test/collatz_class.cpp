#include <iostream>
#include <cassert>
#include "../collatz/collatz.hpp"
#include <gmpxx.h>
#include "../collatz/collatz_affine_map.hpp"



//
// Type Testing
//
template<typename T>
void test_collatz_types() {
    static_assert(FixedWidthIntegral<int>);
    static_assert(FixedWidthIntegral<uint64_t>);
    static_assert(FixedWidthIntegral<uint128_t>);

    static_assert(Int128Integral<int128_t>);
    static_assert(Int128Integral<uint128_t>);
    static_assert(!Int128Integral<uint64_t>);

    static_assert(GMPIntegral<mpz_class>);
    static_assert(!GMPIntegral<uint64_t>);

    static_assert(AnySupportedIntegral<int>);
    static_assert(AnySupportedIntegral<uint128_t>);
    static_assert(AnySupportedIntegral<mpz_class>);
}



//
// Basic Test
//
template<typename T>
void test_collatz_basic() {
    Collatz<T> c(6);

    assert(c.get_initial_value() == 6);
    assert(c.get_peak_value() == 16);  // Collatz(6): 6→3→10→5→16→8→4→2→1
    assert(c.get_step_count() == 8);
    assert(c.get_sequence()[0] == 6);
    assert(c.get_sequence().back() == 1);
    assert(c.get_oe_pattern_string() == "EOEOEEEEO");
    assert(c.get_fg_chain_string() == "GFFGGGF");
    assert(c.get_hwm_index() == 1);  // index where value drops below initial
}



//
// Reset and Reuse An Object
//
template<typename T>
void test_collatz_reset_and_reuse() {
    Collatz<T> c(7);
    assert(c.get_initial_value() == 7);
    assert(c.get_oe_pattern_string() == "OEOEOEEOEEEOEEEEO");
    assert(c.get_fg_chain_string() == "FFFGFGGFGGGF");
    c.init(6);  // Reuse with new value
    assert(c.get_initial_value() == 6);
    assert(c.get_peak_value() == 16);  // Collatz(6): 6→3→10→5→16→8→4→2→1
    assert(c.get_step_count() == 8);
    assert(c.get_sequence()[0] == 6);
    assert(c.get_sequence().back() == 1);
    assert(c.get_oe_pattern_string() == "EOEOEEEEO");
    assert(c.get_hwm_index() == 1);  // index where value drops below initial
}


//
// One Check
//
template<typename T>
void test_collatz_one() {
    Collatz<T> c(1);
    assert(c.get_step_count() == 0);
    assert(c.get_oe_pattern_string() == "O");  // It adds "O" in the last step
}






//
// Run All Helper
//
template<typename T>
void run_all() {
    std::cout << "test_collatz_types ..." << std::flush;
    test_collatz_types<T>();
    std::cout << " passed.\n";

    std::cout << "test_collatz_basic ..." << std::flush;
    test_collatz_basic<T>();
    std::cout << " passed.\n";

    std::cout << "test_collatz_reset_and_reuse() ..." << std::flush;
    test_collatz_reset_and_reuse<T>();
    std::cout << " passed.\n";

    std::cout << "test_collatz_one() ..." << std::flush;
    test_collatz_one<T>();
    std::cout << " passed.\n";
}




int main() {
    std::cout << "=====================" << std::endl;
    std::cout << "Collatz Tests" << std::endl;
    std::cout << "=====================" << std::endl;

    std::cout << "Performing tests with uint64_t." << std::endl;
    run_all<uint64_t>();

    std::cout << "Performing tests with uint128_t." << std::endl;
    run_all<uint128_t>();

    std::cout << "Performing tests with mpz_class." << std::endl;
    run_all<mpz_class>();

    std::cout << "All tests passed.\n";
    return 0;
}
