#include <iostream>
#include <cassert>
#include "../collatz/collatz.hpp" // your class file
// Include GMP if using mpz_class
#include <gmpxx.h>
#include <stdexcept>

void test_with_int() {
    Collatz<uint> c(6, true, true);

    assert(c.get_initial_value() == 6);
    assert(c.get_peak_value() == 16);  // Collatz(6): 6→3→10→5→16→8→4→2→1
    assert(c.get_step_count() == 9);
    assert(c.get_sequence()[0] == 6);
    assert(c.get_sequence().back() == 1);
    assert(c.get_oe_pattern_string() == "EOEOEEEEO");
    assert(c.get_hwm_index() == 1);  // index where value drops below initial
    assert(c.get_sequence_string() == "6, 3, 10, 5, 16, 8, 4, 2, 1");
}

void test_reset_and_reuse() {
    Collatz<uint> c(7, true, true);
    assert(c.get_initial_value() == 7);
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

void test_reset_and_reuse_without_sequence() {
    Collatz<uint> c(7);
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

void test_sequence_and_metadata_default_untracked() {
    Collatz<uint> c(7);
    assert(c.get_track_metadata() == false);
    assert(c.get_track_sequence() == false);
    try {
        assert(c.get_sequence().size() == 0);
        assert(false); // Should not reach here
    } catch (const std::logic_error& e) {
        assert(std::string(e.what()).find("disabled sequence tracking") != std::string::npos);
    }

}

void test_zero() {
    Collatz<uint> c(0, true);
    assert(c.get_sequence().size() == 0);
    assert(c.get_oe_pattern_string().empty());
}

void test_one() {
    Collatz<uint> c(1, true, true);
    assert(c.get_step_count() == 1);
    assert(c.get_oe_pattern_string() == "O");  // It adds "O" in the last step
}

void test_negative_exception() {
    try {
        Collatz<int> c(-5);
        assert(false); // Should not reach here
    } catch (const std::runtime_error& e) {
        assert(std::string(e.what()).find("lower than 0") != std::string::npos);
    }
}

void test_with_mpz() {
    mpz_class val("6");
    Collatz<mpz_class> c(val, true, true);
    assert(c.get_peak_value() == 16);  // Collatz(6): 6→3→10→5→16→8→4→2→1
    assert(c.get_step_count() == 9);
    assert(c.get_sequence()[0] == 6);
    assert(c.get_sequence().back() == 1);
    assert(c.get_oe_pattern_string() == "EOEOEEEEO");
    assert(c.get_hwm_index() == 1);  // index where value drops below initial
    assert(c.get_sequence_string() == "6, 3, 10, 5, 16, 8, 4, 2, 1");
}

int main() {
    test_with_int();
    std::cout << "test_with_int() passed\n";
    test_reset_and_reuse();
    std::cout << "test_reset_and_reuse() passed\n";
    test_reset_and_reuse_without_sequence();
    std::cout << "test_reset_and_reuse_without_sequence() passed\n";
    test_sequence_and_metadata_default_untracked();
    std::cout << "test_sequence_and_metadata_default_untracked() passed\n";
    test_zero();
    std::cout << "test_zero() passed\n";
    test_one();
    std::cout << "test_one() passed\n";
    test_negative_exception();
    std::cout << "test_negative_exception() passed\n";
    test_with_mpz();
    std::cout << "test_with_mpz() passed\n";

    std::cout << "All tests passed.\n";
    return 0;
}
