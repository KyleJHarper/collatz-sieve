#include <cassert>
#include <cstdint>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"
#include "../collatz/string.hpp"
#include <sstream>




template<AnySupportedIntegral T>
void test_string_to_string_any() {
    start_test(__func__);

    // It should print.  Yep.
    std::stringstream stream;
    T value = 42;
    stream << to_string_any(value);
    assert(stream.str() == "42");

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_string_to_string_any<T>();
}



int main() {
    std::string name = "String";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
