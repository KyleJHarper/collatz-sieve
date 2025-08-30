#include <cassert>
#include <cstddef>
#include <cstdint>
#include <array>
#include <iostream>
#include <gmpxx.h>
#include <limits>
#include <stdexcept>
#include "../collatz/sieve.hpp"


//
// All of these tests need to work with native integrals and GMP.
//



//
// Helper for Level 4 Tests
//
const size_t L4_SIZE = 32;
const std::array<uint64_t, L4_SIZE> L4_VALUES = {
    39,
    43,
    47,
    55,
    59,
    63,
    71,
    75,
    79,
    87,
    91,
    95,
    103,
    107,
    111,
    119,
    123,
    127,
    135,
    139,
    143,
    151,
    155,
    159,
    167,
    171,
    175,
    183,
    187,
    191,
    199,
    203,
};



//
// Defaults Regressions
//
template<IntegralOrMPZClass T>
void test_sieve_default_opts() {
    SieveOptions opts;
    assert(opts.pool_size == 1048576);
    assert(opts.tree_opts.track_node_metadata == false);
    assert(opts.tree_opts.prune_hwm_nodes == true);
    assert(opts.tree_opts.prune_parent_levels == true);
    assert(opts.tree_opts.preserve_ancestors == true);
}



//
// Hard code level 4 values for tests.
//
template<IntegralOrMPZClass T>
void test_sieve_level_4_values(Sieve<T>& sieve) {
    T val;
    sieve.next(val); assert(val == L4_VALUES[0]);
    sieve.next(val); assert(val == L4_VALUES[1]);
    sieve.next(val); assert(val == L4_VALUES[2]);
    sieve.next(val); assert(val == L4_VALUES[3]);
    sieve.next(val); assert(val == L4_VALUES[4]);
    sieve.next(val); assert(val == L4_VALUES[5]);
    sieve.next(val); assert(val == L4_VALUES[6]);

    // Calling refill without exhasuting the pool shouldn't be an issue.
    assert(sieve.get_pool_index() != 0);
    sieve.refill_pool();
    assert(sieve.get_pool_index() == 0);

    // Values should continue on without missing a beat.
    sieve.next(val); assert(val == L4_VALUES[7]);
    sieve.next(val); assert(val == L4_VALUES[8]);
    sieve.next(val); assert(val == L4_VALUES[9]);
    sieve.next(val); assert(val == L4_VALUES[10]);

    // Refilling repeatedly should be safe.
    sieve.refill_pool();
    sieve.refill_pool();
    sieve.refill_pool();
    sieve.refill_pool();
    sieve.next(val); assert(val == L4_VALUES[11]);
    sieve.next(val); assert(val == L4_VALUES[12]);
    sieve.next(val); assert(val == L4_VALUES[13]);
}



//
// L4 Internals Should Match
//
template<IntegralOrMPZClass T>
void test_sieve_l4_internals(Sieve<T>& sieve, size_t next_calls, bool bulk_fill_overflowed = false) {
    // Only works for l4 trees.
    assert(sieve.get_tree_level_count() == 4);

    // Does NOT work if premature refills() were called.
    assert(sieve.get_pool_premature_refills() == 0);

    // Refills should match: floor(next_calls / pool_size) + 1
    // Only if bulk fills didn't overflow.
    if (! bulk_fill_overflowed) {
        assert(sieve.get_pool_refills() == std::floor(next_calls / sieve.get_pool_size()) + 1);
    }

    // L4 survivor count is 3.
    assert(sieve.get_survivor_count() == 3);

    // Step is 2^(L) ==> 2^(4) ==> 16
    assert(sieve.get_step() == 16);
    assert(sieve.get_step() == BinaryTreeMath<T>::st_step(sieve.get_tree_level_count()));

    // Pool index math: next_calls % pool_size
    if (bulk_fill_overflowed) {
        assert(0 == sieve.get_pool_index());
    } else {
        assert(next_calls % sieve.get_pool_size() == sieve.get_pool_index());
    }

    // Multiplier is tied to pool filling.
    // Math: floor(pool_generated_count / survivor_count) + 1
    // Math falls apart if bulk overflows happened.
    if (! bulk_fill_overflowed) {
        size_t expected_generated = (std::floor(next_calls / sieve.get_pool_size()) + 1) * sieve.get_pool_size();
        assert(sieve.get_pool_generated_count() == expected_generated);
        size_t expected_multiplier = std::floor(expected_generated / sieve.get_survivor_count()) + 1;
        assert(std::floor(sieve.get_pool_generated_count() / sieve.get_survivor_count()) + 1 == expected_multiplier);
    }
}



//
// Pool filling should work under all circumstances.
//
template<IntegralOrMPZClass T>
void test_sieve_pool_filling() {
    SieveOptions opts;
    opts.pool_size = 64;
    Sieve<T> sieve(4, opts);
    test_sieve_level_4_values(sieve);
}



//
// Tree source shouldn't matter.
//
template<IntegralOrMPZClass T>
void test_sieve_tree_source() {
    SieveOptions opts;
    BinaryTree<T> tree(4, opts.tree_opts);
    Sieve<T> sieve(tree);
    test_sieve_level_4_values(sieve);
    Sieve<T> sieve2(4);
    test_sieve_level_4_values(sieve2);
}



//
// Bulk Next
//
template<IntegralOrMPZClass T>
void test_sieve_bulk_next() {
    SieveOptions opts;
    opts.pool_size = 64;
    BinaryTree<T> tree(4, opts.tree_opts);
    Sieve<T> sieve(tree, opts);
    std::vector<T> bulk;
    size_t next_count = 20;
    T x;

    // Behavior: we will NOT change your size() data.  We will error.
    sieve.init(&tree, opts);
    bulk.clear();
    bulk.shrink_to_fit();
    bulk.reserve(next_count);
    assert(bulk.capacity() == next_count);
    assert(bulk.size() == 0);
    try {
        sieve.next(bulk);
        assert(false); // Should throw
    } catch (const std::logic_error& e) {
        assert(std::string(e.what()).find("You must resize() your vector buffer before we'll") != std::string::npos);
    }

    // A vector smaller than pool has left.
    sieve.init(&tree, opts);
    bulk.clear();
    bulk.shrink_to_fit();
    bulk.resize(next_count);
    assert(bulk.capacity() == next_count);
    assert(bulk.size() == next_count);
    sieve.next(bulk);
    assert(bulk.size() == next_count);
    assert(sieve.get_pool_index() == next_count);
    for (size_t i = 0; i < 20; i++) {
        assert(bulk[i] == L4_VALUES[i]);
    }
    assert(sieve.get_pool().at(next_count) == L4_VALUES[next_count]);
    test_sieve_l4_internals(sieve, next_count, false);

    // A vector same size as pool has left.
    next_count = 64;
    sieve.init(&tree, opts);
    bulk.clear();
    bulk.shrink_to_fit();
    bulk.resize(next_count);
    assert(bulk.capacity() == next_count);
    assert(bulk.size() == next_count);
    sieve.next(bulk);
    assert(bulk.size() == next_count);
    assert(sieve.get_pool_index() == 0);  // Should reset/wrap to 0.
    for (size_t i = 0; i < 20; i++) {
        assert(bulk[i] == L4_VALUES[i]);
    }
    test_sieve_l4_internals(sieve, next_count, true);

    // A vector larger than pool has left, should wrap.
    next_count = 68;
    sieve.init(&tree, opts);
    bulk.clear();
    bulk.shrink_to_fit();
    bulk.resize(next_count);
    assert(bulk.capacity() == next_count);
    assert(bulk.size() == next_count);
    // With next(T&) we get an index of 4: 68 % 64 == 4.
    for (size_t i = 0; i < next_count; i++) {
        sieve.next(x);
        if (i <= 20) {
            assert(x == L4_VALUES[i]);
        }
    }
    assert(sieve.get_pool_index() == 4);
    // Reset and redo test with bulk.  Next(vector&) should direct fill and leave pool index at 0.
    sieve.init(&tree, opts);
    bulk.clear();
    bulk.shrink_to_fit();
    bulk.resize(next_count);
    sieve.next(bulk);
    assert(bulk.size() == next_count);
    assert(sieve.get_pool_index() == 0);
    for (size_t i = 0; i < 20; i++) {
        assert(bulk[i] == L4_VALUES[i]);
    }
    test_sieve_l4_internals(sieve, next_count, true);

    // A massive vector requiring multiple refills.
    next_count = 6003;
    sieve.init(&tree, opts);
    bulk.clear();
    bulk.shrink_to_fit();
    bulk.resize(next_count);
    assert(bulk.capacity() == next_count);
    assert(bulk.size() == next_count);
    // With next(T&) we get an index of 4: 6003 % 64 == 51.
    for (size_t i = 0; i < next_count; i++) {
        sieve.next(x);
        if (i <= 20) {
            assert(x == L4_VALUES[i]);
        }
    }
    assert(sieve.get_pool_index() == 51);
    // Reset and redo test with bulk.  Next(vector&) should direct fill and leave pool index at 0.
    sieve.init(&tree, opts);
    bulk.clear();
    bulk.shrink_to_fit();
    bulk.resize(next_count);
    sieve.next(bulk);
    assert(bulk.size() == next_count);
    assert(sieve.get_pool_index() == 0);
    for (size_t i = 0; i < 20; i++) {
        assert(bulk[i] == L4_VALUES[i]);
    }
    test_sieve_l4_internals(sieve, next_count, true);
}



//
// Above 64 bit
//
template<IntegralOrMPZClass T>
void test_sieve_over_64_bit() {
    // Build the sieve, and then coerce the step for testing.
    SieveOptions opts;
    Sieve<T> sieve(4, opts);

    // Detect overflow or error
    if constexpr(std::integral<T>) {
        assert(sieve.get_max_multiplier() == 1152921504606846976);
        try {
            sieve.unsafe_coerce_multiplier(sieve.get_max_multiplier());
            sieve.bump_survivor_index(sieve.get_survivor_count() - sieve.get_survivor_index());
            assert(false);
        } catch (const std::overflow_error& e) {
            assert(std::string(e.what()).find("exceeeds max allowed multiplier for 64-bit type") != std::string::npos);
        }
    } else {
        assert(sieve.get_max_multiplier() == std::numeric_limits<uint64_t>::max());
        try {
            sieve.unsafe_coerce_multiplier(sieve.get_max_multiplier());
            sieve.bump_survivor_index(sieve.get_survivor_count() - sieve.get_survivor_index());
            assert(false);
        } catch (const std::overflow_error& e) {
            assert(std::string(e.what()).find("exceeeds max allowed multiplier for GMP type") != std::string::npos);
        }
    }
}




//
// Wrapper to run all tests.
//
template<IntegralOrMPZClass T>
void run_all() {
    std::cout << "test_sieve_default_opts ..." << std::flush;
    test_sieve_default_opts<T>();
    std::cout << " passed.\n";

    std::cout << "test_sieve_pool_filling ..." << std::flush;
    test_sieve_pool_filling<T>();
    std::cout << " passed.\n";

    std::cout << "test_sieve_tree_source ..." << std::flush;
    test_sieve_tree_source<T>();
    std::cout << " passed.\n";

    std::cout << "test_sieve_bulk_next ..." << std::flush;
    test_sieve_bulk_next<T>();
    std::cout << " passed.\n";

    std::cout << "test_sieve_over_64_bit ..." << std::flush;
    test_sieve_over_64_bit<T>();
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
