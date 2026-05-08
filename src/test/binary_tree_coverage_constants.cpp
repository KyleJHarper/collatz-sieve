#include <cassert>
#include <cstdint>
#include "helpers.hpp"
#include "../collatz/concepts.hpp"
#include "../collatz/binary_tree_coverage_constants.hpp"



template<AnySupportedIntegral T>
void test_binary_tree_coverage_constants_statics() {
    start_test(__func__);

    // Test the constants just to ensure they're present and reasonable.
    assert(BinaryTreeCoverageConstants::MAX_KNOWN_COVERAGE_LEVEL >= 45);

    // Known map should match size with the above.
    assert(BinaryTreeCoverageConstants::KNOWN_COVERAGE_MAP.size() == BinaryTreeCoverageConstants::MAX_KNOWN_COVERAGE_LEVEL + 1);

    // Getting a known level should return a positive value after level 1.
    for (level_t level = 2; level <= BinaryTreeCoverageConstants::MAX_KNOWN_COVERAGE_LEVEL; level++) {
        assert(BinaryTreeCoverageConstants::get_known_coverage<T>(level) > 0);
        assert(BinaryTreeCoverageConstants::get_known_coverage_sum_to_level<T>(level) > 0);
    }

    // Getting the totals should always be positive.  It should match the BinaryTreeMath output too.
    for (level_t level = 1; level <= BinaryTreeCoverageConstants::MAX_KNOWN_COVERAGE_LEVEL; level++) {
        assert(BinaryTreeCoverageConstants::get_total<T>(level) > 0);
        assert(BinaryTreeCoverageConstants::get_total<T>(level) == BinaryTreeMath<T>::st_node_count_of_level(level));
        assert(BinaryTreeCoverageConstants::get_total_sum_of_levels<T>(1, level) > 0);
        assert(BinaryTreeCoverageConstants::get_total_sum_of_levels<T>(1, level) == BinaryTreeMath<T>::st_node_count_of_levels(1, level));
    }

    end_test();
}




template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();
    test_binary_tree_coverage_constants_statics<T>();
}



int main() {
    std::string name = "BinaryTreeCoverageConstants";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
