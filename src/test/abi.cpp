#include <cassert>
#include <cstdint>
#include "helpers.hpp"
#include "../collatz/concepts.hpp"
#include "../collatz/abi.hpp"



template<AnySupportedIntegral T>
void test_abi_demangle() {
    start_test(__func__);

    // Can't really tests for much since these are compiler-specific.  Make sure it isn't blank and doesn't throw.
    assert(ABI::demangle<T>() != "");
    // Sending an unknown (garbage) should return it as is.
    std::string not_a_real_type_string = "sklfjwjf20dj2wqdjdwq";
    assert(ABI::demangle(not_a_real_type_string.c_str()) == not_a_real_type_string);

    end_test();
}



template<AnySupportedIntegral T>
void test_abi_cache_line_size() {
    start_test(__func__);

    // It's hard coded to 64.
    assert(ABI::CACHE_LINE_SIZE == 64);

    end_test();
}



template<AnySupportedIntegral T>
void test_abi_get_bit_width_id() {
    start_test(__func__);

    // The mpz class should get assigned a fixed value.
    assert(ABI::MPZ_CLASS_BIT_WIDTH_ID == 0);

    // All other types should be equal to whatever their sizeof() returns.
    if constexpr(FixedWidthIntegral<T>) {
        assert(ABI::get_bit_width_id<T>() == sizeof(T) * 8);
    } else if constexpr(GMPIntegral<T>) {
        assert(ABI::get_bit_width_id<T>() == 0);
        assert(ABI::get_bit_width_id<T>() == ABI::MPZ_CLASS_BIT_WIDTH_ID);
    }

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();
    test_abi_demangle<T>();
    test_abi_cache_line_size<T>();
    test_abi_get_bit_width_id<T>();
}



int main() {
    std::string name = "ABI";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
