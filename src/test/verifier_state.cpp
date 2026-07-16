#include <cassert>
#include <cstdint>
#include "../collatz/concepts.hpp"
#include "../collatz/verifier_state.hpp"
#include "helpers.hpp"




template<AnySupportedIntegral T>
void test_verifier_state_enum() {
    start_test(__func__);

    // Just ensure all the members exist and have known values.
    assert(static_cast<uint32_t>(VerifierState::STOPPING) == 1);
    assert(static_cast<uint32_t>(VerifierState::STOPPED) == 2);
    assert(static_cast<uint32_t>(VerifierState::RUNNING) == 3);
    assert(static_cast<uint32_t>(VerifierState::PAUSED) == 4);

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_verifier_state_enum<T>();
}



int main() {
    std::string name = "VerifierState";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
