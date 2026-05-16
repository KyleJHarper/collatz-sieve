#include <cassert>
#include <cstdint>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"
#include "../collatz/udl.hpp"


template<AnySupportedIntegral T>
void test_udl_mpz() {
    start_test(__func__);

    if constexpr(GMPIntegral<T>) {
        T value = 42;
        T value_from_udl = "42"_mpz;
        assert(value == value_from_udl);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_udl_u128() {
    start_test(__func__);

    if constexpr(Int128Integral<T>) {
        T value = 42;
        T value_from_udl = "42"_u128;
        assert(value == value_from_udl);
    }

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_udl_mpz<T>();
    test_udl_u128<T>();
}



int main() {
    std::string name = "UDLs";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
