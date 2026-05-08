#include <cassert>
#include <cstdint>
#include "helpers.hpp"
#include "../collatz/concepts.hpp"
#include "../collatz/binary_tree_coverage.hpp"




template<AnySupportedIntegral T>
void test_binary_tree_coverage_basics() {
    start_test(__func__);

    BinaryTreeCoverage<T> coverage;
    assert(coverage.get_covered() == 0);
    assert(coverage.get_uncovered() == 0);
    assert(coverage.get_total() == 0);
    try {
        mpf_class x = coverage.get_ratio();
        assert(false);
    } catch (const std::out_of_range& e) {
        assert(std::string(e.what()).find("You cannot call get_ratio() when 'total' is 0 or less.") != std::string::npos);
    }

    // Setting
    coverage.set_covered(42);
    assert(coverage.get_covered() == 42);
    coverage.set_total(50);
    assert(coverage.get_total() == 50);
    assert(coverage.get_uncovered() == 8);
    assert(coverage.get_ratio() > 0.83);
    assert(coverage.get_ratio() < 0.85);

    // Adding
    coverage.add_covered(3);
    assert(coverage.get_covered() == 45);
    assert(coverage.get_uncovered() == 5);
    coverage.add_total(10);
    assert(coverage.get_total() == 60);
    assert(coverage.get_uncovered() == 15);
    assert(coverage.get_ratio() > 0.74);
    assert(coverage.get_ratio() < 0.76);

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_coverage_merge() {
    start_test(__func__);

    BinaryTreeCoverage<T> a;
    BinaryTreeCoverage<T> b;

    a.set_covered(400);
    a.set_total(1000);
    b.set_covered(50);
    b.set_total(100);

    a.merge(b);
    assert(a.get_covered() == 450);
    assert(a.get_total() == 1100);
    assert(a.get_uncovered() == 650);

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_coverage_cloneable() {
    start_test(__func__);

    const T covered = 42;
    const T total = 53;
    BinaryTreeCoverage<T> coverage;
    coverage.set_covered(covered);
    coverage.set_total(total);
    assert(cloneable(coverage));

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_coverage_equality() {
    start_test(__func__);

    const T covered = 42;
    const T total = 53;

    // Build Objects
    BinaryTreeCoverage<T> coverage;
    coverage.set_covered(covered);
    coverage.set_total(total);
    BinaryTreeCoverage<T> coverage2;
    coverage2.set_covered(covered);
    coverage2.set_total(total);

    // Test
    assert(equality(coverage, coverage2));

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();
    test_binary_tree_coverage_basics<T>();
    test_binary_tree_coverage_merge<T>();
    test_binary_tree_coverage_cloneable<T>();
    test_binary_tree_coverage_equality<T>();
}



int main() {
    std::string name = "BinaryTreeCoverage";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
