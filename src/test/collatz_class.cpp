#include <iostream>
#include <cassert>
#include "../collatz/collatz.hpp"
#include <gmpxx.h>
#include <stdexcept>



//
// Basic Test
//
template<typename T>
void test_collatz_basic() {
    Collatz<T> c(6, true, true);

    assert(c.get_initial_value() == 6);
    assert(c.get_peak_value() == 16);  // Collatz(6): 6→3→10→5→16→8→4→2→1
    assert(c.get_step_count() == 9);
    assert(c.get_sequence()[0] == 6);
    assert(c.get_sequence().back() == 1);
    assert(c.get_oe_pattern_string() == "EOEOEEEEO");
    assert(c.get_fg_pattern_string() == "GFFGGGF");
    assert(c.get_hwm_index() == 1);  // index where value drops below initial
    assert(c.get_sequence_string() == "6, 3, 10, 5, 16, 8, 4, 2, 1");
}



//
// Reset and Reuse An Object
//
template<typename T>
void test_collatz_reset_and_reuse() {
    Collatz<T> c(7, true, true);
    assert(c.get_initial_value() == 7);
    assert(c.get_oe_pattern_string() == "OEOEOEEOEEEOEEEEO");
    assert(c.get_fg_pattern_string() == "FFFGFGGFGGGF");
    c.init(6, true, true);  // Reuse with new value
    assert(c.get_initial_value() == 6);
    assert(c.get_peak_value() == 16);  // Collatz(6): 6→3→10→5→16→8→4→2→1
    assert(c.get_step_count() == 9);
    assert(c.get_sequence()[0] == 6);
    assert(c.get_sequence().back() == 1);
    assert(c.get_oe_pattern_string() == "EOEOEEEEO");
    assert(c.get_hwm_index() == 1);  // index where value drops below initial
    assert(c.get_sequence_string() == "6, 3, 10, 5, 16, 8, 4, 2, 1");
}



//
// Reset and Reuse An Object without Sequence Data
//
template<typename T>
void test_collatz_reset_and_reuse_without_sequence() {
    Collatz<T> c(7);
    try {
        assert(c.get_peak_value() > 1);
        assert(false); // Should not reach here
    } catch (const std::logic_error& e) {
        assert(std::string(e.what()).find("disabled metadata when") != std::string::npos);
    }
    assert(c.get_initial_value() == 7);
    c.init(6, false, true);  // Reuse with new value
    assert(c.get_initial_value() == 6);
    assert(c.get_peak_value() == 16);  // Collatz(6): 6→3→10→5→16→8→4→2→1
    assert(c.get_step_count() == 9);
    try {
        assert(c.get_sequence()[0] == 6);
        assert(false); // Should not reach here
    } catch (const std::logic_error& e) {
        assert(std::string(e.what()).find("disabled sequence tracking") != std::string::npos);
    }
    assert(c.get_oe_pattern_string() == "EOEOEEEEO");
    assert(c.get_hwm_index() == 1);  // index where value drops below initial
}



//
// Sequence and Metadata Defaults
//
template<typename T>
void test_collatz_sequence_and_metadata_default_untracked() {
    Collatz<T> c(7);
    assert(c.get_track_metadata() == false);
    assert(c.get_track_sequence() == false);
    try {
        assert(c.get_sequence().size() == 0);
        assert(false); // Should not reach here
    } catch (const std::logic_error& e) {
        assert(std::string(e.what()).find("disabled sequence tracking") != std::string::npos);
    }

}



//
// Zero Check
//
template<typename T>
void test_collatz_zero() {
    Collatz<T> c(0, true);
    assert(c.get_sequence().size() == 0);
    assert(c.get_oe_pattern_string().empty());
}



//
// One Check
//
template<typename T>
void test_collatz_one() {
    Collatz<T> c(1, true, true);
    assert(c.get_step_count() == 1);
    assert(c.get_oe_pattern_string() == "O");  // It adds "O" in the last step
}



//
// Run All Helper
//
template<typename T>
void run_all() {
    std::cout << "test_collatz_basic ..." << std::flush;
    test_collatz_basic<T>();
    std::cout << " passed.\n";

    std::cout << "test_collatz_reset_and_reuse() ..." << std::flush;
    test_collatz_reset_and_reuse<T>();
    std::cout << " passed.\n";

    std::cout << "test_collatz_reset_and_reuse_without_sequence() ..." << std::flush;
    test_collatz_reset_and_reuse_without_sequence<T>();
    std::cout << " passed.\n";

    std::cout << "test_collatz_sequence_and_metadata_default_untracked() ..." << std::flush;
    test_collatz_sequence_and_metadata_default_untracked<T>();
    std::cout << " passed.\n";

    std::cout << "test_collatz_zero() ..." << std::flush;
    test_collatz_zero<T>();
    std::cout << " passed.\n";

    std::cout << "test_collatz_one() ..." << std::flush;
    test_collatz_one<T>();
    std::cout << " passed.\n";
}




int main() {
    std::cout << "Performing tests with uint64_t." << std::endl;
    run_all<uint64_t>();

    std::cout << "Performing tests with mpz_class." << std::endl;
    run_all<mpz_class>();

    std::cout << "All tests passed.\n";
    return 0;
}
