#include <cassert>
#include <cstdint>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"
#include "../collatz/binary_tree_options.hpp"




template<AnySupportedIntegral T>
void test_binary_tree_options_defaults() {
    start_test(__func__);

    BinaryTreeOptions opts;
    assert(opts.prune_hwm_nodes == false);
    assert(opts.prune_parent_levels == false);
    assert(opts.preserve_ancestors == false);
    assert(opts.verify_non_hwm_nodes == false);

    // Change them from defaults.
    opts.prune_hwm_nodes = true;
    opts.prune_parent_levels = true;
    opts.preserve_ancestors = true;
    opts.verify_non_hwm_nodes = true;
    assert(opts.prune_hwm_nodes == true);
    assert(opts.prune_parent_levels == true);
    assert(opts.preserve_ancestors == true);
    assert(opts.verify_non_hwm_nodes == true);

    // Now reset them
    opts.reset();
    assert(opts.prune_hwm_nodes == false);
    assert(opts.prune_parent_levels == false);
    assert(opts.preserve_ancestors == false);
    assert(opts.verify_non_hwm_nodes == false);

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();
    test_binary_tree_options_defaults<T>();
}



int main() {
    std::string name = "BinaryTreeOptions";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
