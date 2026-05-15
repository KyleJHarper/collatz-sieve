#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"
#include "../collatz/gmp.hpp"




template<AnySupportedIntegral T>
void test_gmp_deep_sizeof() {
    start_test(__func__);

    mpz_class x = 42;
    mpf_class f("42.4242424242424242424242424242414124234235");

    assert(GMP::deep_sizeof(x) > 1);
    assert(GMP::deep_sizeof(f) > 1);

    end_test();
}



template<AnySupportedIntegral T>
void test_gmp_mpz_get_ui64() {
    start_test(__func__);

    uint64_t u_value = 1234567890;
    mpz_class big_mpz(1);
    big_mpz <<= 65;
    big_mpz |= u_value;
    assert(big_mpz != u_value);
    uint64_t ui64_extracted = GMP::mpz_get_ui64(big_mpz.get_mpz_t());
    assert(ui64_extracted == u_value);

    u_value = std::numeric_limits<uint64_t>::max();
    big_mpz = 1;
    big_mpz <<= 65;
    big_mpz |= u_value;
    ui64_extracted = GMP::mpz_get_ui64(big_mpz.get_mpz_t());
    assert(ui64_extracted == u_value);

    end_test();
}



template<AnySupportedIntegral T>
void test_gmp_parse_mpz() {
    start_test(__func__);

    mpz_class x = GMP::parse_mpz("42");
    mpz_class y = GMP::parse_mpz("123443564645634984723984723473492837492837428092349803248748923748927342348234832894723442");

    try {
        mpz_class z = GMP::parse_mpz("234 NOPE 234");
    } catch (std::invalid_argument& e) {
        assert(std::string(e.what()).find("invalid digit in _mpz literal") != std::string::npos);
    }

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_gmp_deep_sizeof<T>();
    test_gmp_mpz_get_ui64<T>();
    test_gmp_parse_mpz<T>();
}



int main() {
    std::string name = "GMP";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
