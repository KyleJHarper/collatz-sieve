#include <cassert>
#include <cstdint>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"
#include "../collatz/for_each.hpp"



template<AnySupportedIntegral T>
void test_for_each_policy() {
    start_test(__func__);

    assert(static_cast<int>(ForEachPolicy::SERIAL) == 1);
    assert(static_cast<int>(ForEachPolicy::PARALLEL) == 2);

    end_test();
}



template<AnySupportedIntegral T>
void test_for_each_signal() {
    start_test(__func__);

    assert(static_cast<int>(ForEachSignal::CONTINUE) == 1);
    assert(static_cast<int>(ForEachSignal::BREAK) == 2);

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_for_each_policy<T>();
    test_for_each_signal<T>();
}



int main() {
    std::string name = "ForEach";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
