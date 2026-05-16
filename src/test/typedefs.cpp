#include <cassert>
#include <cstdint>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"




template<AnySupportedIntegral T>
void test_typedefs_all() {
    start_test(__func__);

    assert((std::is_same_v<uint32_t, level_t>));
    assert((std::is_same_v<uint32_t, seq_size_t>));

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_typedefs_all<T>();
}



int main() {
    std::string name = "Type Defs";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
