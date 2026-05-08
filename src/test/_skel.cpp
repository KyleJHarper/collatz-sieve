#include <cassert>
#include <cstdint>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"







template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();
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
