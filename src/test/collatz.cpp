#include <cassert>
#include <cstdint>
#include <limits>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"
#include "../collatz/collatz.hpp"



template<AnySupportedIntegral T>
void test_collatz_basic_construction() {
    start_test(__func__);

    // Default initialization should have default values, of course.
    Collatz<T> collatz;
    assert(collatz.get_initial_value() == 0);
    assert(collatz.get_peak_value() == 0);
    assert(collatz.get_hwm_index() == 0);
    assert(collatz.get_step_count() == 0);
    assert(collatz.is_initialized() == false);
    assert(collatz.has_overflowed() == false);

    // Test with 1 because it should do almost nothing.
    Collatz<T> collatz_1(1);
    assert(collatz_1.get_initial_value() == 1);
    assert(collatz_1.get_peak_value() == 1);
    assert(collatz_1.get_hwm_index() == 0);
    assert(collatz_1.get_step_count() == 0);
    assert(collatz_1.is_initialized() == true);
    assert(collatz_1.has_overflowed() == false);

    // Zero is invalid in Collatz, but this API supports is as a special case.
    Collatz<T> collatz_0(0);
    assert(collatz_0.get_sequence().size() == 0);
    assert(collatz_0.get_oe_pattern_string().empty());

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_init() {
    start_test(__func__);

    // Init after default construction should work.
    Collatz<T> collatz;
    assert(collatz.get_initial_value() == 0);
    assert(collatz.get_peak_value() == 0);
    assert(collatz.get_hwm_index() == 0);
    assert(collatz.get_step_count() == 0);
    assert(collatz.is_initialized() == false);
    assert(collatz.has_overflowed() == false);
    // Now init.
    collatz.init(1);
    assert(collatz.get_initial_value() == 1);
    assert(collatz.get_peak_value() == 1);
    assert(collatz.get_hwm_index() == 0);
    assert(collatz.get_step_count() == 0);
    assert(collatz.is_initialized() == true);
    assert(collatz.has_overflowed() == false);

    // Overly large values will overflow.
    if constexpr(FixedWidthIntegral<T>) {
        // The max is always odd, and will 3x immediately, overflowing.
        T big_odd_value = std::numeric_limits<T>::max();
        try {
            Collatz<T> overflowed_sequence(big_odd_value);
            assert(false);
        } catch (CollatzSequenceOverflow& e) {
            assert(std::string(e.what()).find("Overflow when building for_each_sequence_step") != std::string::npos);
        }

        // Perform with init and confirm has_overflowed() is true.
        Collatz<T> will_overflow;
        try {
            will_overflow.init(big_odd_value);
            assert(false);
        } catch (CollatzSequenceOverflow& e) {
            assert(std::string(e.what()).find("Overflow when building for_each_sequence_step") != std::string::npos);
        }
        assert(will_overflow.has_overflowed() == true);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_reset() {
    start_test(__func__);

    // Make a default.
    Collatz<T> collatz;
    assert(collatz.get_initial_value() == 0);
    assert(collatz.get_peak_value() == 0);
    assert(collatz.get_hwm_index() == 0);
    assert(collatz.get_step_count() == 0);
    assert(collatz.is_initialized() == false);
    assert(collatz.has_overflowed() == false);

    // Re-init to 1
    collatz.init(1);
    assert(collatz.get_initial_value() == 1);
    assert(collatz.get_peak_value() == 1);
    assert(collatz.get_hwm_index() == 0);
    assert(collatz.get_step_count() == 0);
    assert(collatz.is_initialized() == true);
    assert(collatz.has_overflowed() == false);

    // Reset and test
    collatz.reset();
    assert(collatz.get_initial_value() == 0);
    assert(collatz.get_peak_value() == 0);
    assert(collatz.get_hwm_index() == 0);
    assert(collatz.get_step_count() == 0);
    assert(collatz.is_initialized() == false);
    assert(collatz.has_overflowed() == false);

    // Overflow it, then reset
    if constexpr(FixedWidthIntegral<T>) {
        // The max is always odd, and will 3x immediately, overflowing.
        T big_odd_value = std::numeric_limits<T>::max();
        try {
            collatz.init(big_odd_value);
        } catch (...) {
            // Ignore it
        }
        assert(collatz.has_overflowed() == true);

        collatz.reset();
        assert(collatz.get_initial_value() == 0);
        assert(collatz.get_peak_value() == 0);
        assert(collatz.get_hwm_index() == 0);
        assert(collatz.get_step_count() == 0);
        assert(collatz.is_initialized() == false);
        assert(collatz.has_overflowed() == false);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_get_sequence() {
    start_test(__func__);

    Collatz<T> collatz;
    std::vector<T> manual_sequence;
    for (T i = 1; i < 100000; i++) {
        T tmp = i;
        manual_sequence.clear();
        manual_sequence.push_back(tmp);
        while(tmp > 1) {
            if (tmp % 2 == 1) {
                tmp = (3 * tmp) + 1;
            } else {
                tmp = tmp / 2;
            }
            manual_sequence.push_back(tmp);
        }
        collatz.init(i);
        std::vector<T> obj_sequence = collatz.get_sequence();
        assert(manual_sequence.size() == obj_sequence.size());
        for (size_t j = 0; j < manual_sequence.size(); j++) {
            assert(manual_sequence.at(j) == obj_sequence.at(j));
        }
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_st_verify() {
    start_test(__func__);

    // Default sentinel value of 1.
    for(T i = 1; i < 100000; i++) {
        assert(Collatz<T>::st_verify(i));
    }

    // Values which overflow are okay.
    if constexpr(FixedWidthIntegral<T>) {
        T value_that_overflows = std::numeric_limits<T>::max();
        assert(Collatz<T>::st_verify(value_that_overflows));
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_st_verify_unsafe() {
    start_test(__func__);

    // Run through it.  Requires sentinel value to be 3+.
    for(T i = 3; i < 100000; i++) {
        assert(Collatz<T>::st_verify_unsafe(i, i));
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_st_for_each_sequence_step() {
    start_test(__func__);

    T manual_step = 0;

    // Static version.
    for (T i = 1; i < 100000; i++) {
        manual_step = i;
        Collatz<T>::st_for_each_sequence_step(i, [&](const T& step) {
            assert(manual_step == step);
            if (manual_step % 2 == 1) {
                manual_step = (manual_step * 3) + 1;
            } else {
                manual_step = manual_step / 2;
            }
            return false;
        });
    }

    // Member version.  Uses member's _initial_value.
    Collatz<T> collatz;
    for (T i = 1; i < 100000; i++) {
        manual_step = i;
        collatz.init(i);
        collatz.for_each_sequence_step([&](const T& step) {
            assert(manual_step == step);
            if (manual_step % 2 == 1) {
                manual_step = (manual_step * 3) + 1;
            } else {
                manual_step = manual_step / 2;
            }
            return false;
        });
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_st_get_step_count() {
    start_test(__func__);

    seq_size_t manual_step_count;
    T manual_step;
    for (T i = 1; i < 100000; i++) {
        manual_step = i;
        manual_step_count = 0;
        while (manual_step > 1) {
            if (manual_step % 2 == 1) {
                manual_step = (manual_step * 3) + 1;
            } else {
                manual_step = manual_step / 2;
            }
            manual_step_count++;
        }
        assert(Collatz<T>::st_get_step_count(i) == manual_step_count);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_st_get_peak() {
    start_test(__func__);

    T peak;
    T manual_step;
    T out;
    for (T i = 1; i < 100000; i++) {
        manual_step = i;
        peak = manual_step;
        while (manual_step > 1) {
            if (manual_step % 2 == 1) {
                manual_step = (manual_step * 3) + 1;
            } else {
                manual_step = manual_step / 2;
            }
            if (manual_step > peak) {
                peak = manual_step;
            }
        }
        Collatz<T>::st_get_peak(i, out);
        assert(out == peak);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_st_for_each_fg_chain_link() {
    start_test(__func__);

    T manual_step = 0;
    bool manual_is_F;
    for (T i = 1; i < 100000; i++) {
        manual_step = i;
        manual_is_F = i % 2 == 1;
        Collatz<T>::st_for_each_fg_chain_link(i, [&](bool is_F) {
            manual_is_F = manual_step % 2 == 1;
            assert(manual_is_F == is_F);
            if (manual_is_F) {
                manual_step = (manual_step * 3) + 1;
                manual_step = manual_step / 2;
            } else {
                manual_step = manual_step / 2;
            }
            return false;
        });
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_st_for_each_fg_step() {
    start_test(__func__);

    T manual_step = 0;
    for (T i = 1; i < 100000; i++) {
        manual_step = i;
        Collatz<T>::st_for_each_fg_step(i, [&](const T& step) {
            assert(manual_step == step);
            if (manual_step % 2 == 1) {
                manual_step = (manual_step * 3) + 1;
                manual_step = manual_step / 2;
            } else {
                manual_step = manual_step / 2;
            }
            return false;
        });
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_st_get_fg_chain_string() {
    start_test(__func__);

    // Static and member methods
    std::string manual_fg_string;
    Collatz<T> collatz;
    for (T i = 1; i < 100000; i++) {
        manual_fg_string.clear();
        Collatz<T>::st_for_each_fg_chain_link(i, [&](bool is_F) {
            manual_fg_string += (is_F ? 'F' : 'G');
            return false;
        });
        collatz.init(i);
        assert(Collatz<T>::st_get_fg_chain_string(i) == manual_fg_string);
        assert(collatz.get_fg_chain_string() == manual_fg_string);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_st_get_oe_pattern_string() {
    start_test(__func__);

    // Static and member methods
    std::string manual_oe_string;
    Collatz<T> collatz;
    for (T i = 1; i < 100000; i++) {
        manual_oe_string.clear();
        Collatz<T>::st_for_each_sequence_step(i, [&](const T& step) {
            manual_oe_string += (step % 2 == 1 ? 'O' : 'E');
            return false;
        });
        collatz.init(i);
        assert(Collatz<T>::st_get_oe_pattern_string(i) == manual_oe_string);
        assert(collatz.get_oe_pattern_string() == manual_oe_string);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_collatz_st_fg_to_oe() {
    start_test(__func__);

    Collatz<T> collatz;
    for (T i = 1; i < 100000; i++) {
        collatz.init(i);
        assert(collatz.get_oe_pattern_string() == Collatz<T>::st_fg_to_oe(collatz.get_fg_chain_string()));
    }

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_collatz_basic_construction<T>();
    test_collatz_init<T>();
    test_collatz_reset<T>();
    test_collatz_get_sequence<T>();
    test_collatz_st_verify<T>();
    test_collatz_st_verify_unsafe<T>();
    test_collatz_st_for_each_sequence_step<T>();
    test_collatz_st_get_step_count<T>();
    test_collatz_st_get_peak<T>();
    test_collatz_st_for_each_fg_chain_link<T>();
    test_collatz_st_for_each_fg_step<T>();
    test_collatz_st_get_fg_chain_string<T>();
    test_collatz_st_get_oe_pattern_string<T>();
    test_collatz_st_fg_to_oe<T>();
}



int main() {
    std::string name = "Collatz";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
