#include <cassert>
#include <cstdint>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"




void todo() {
    //
    // Bit Reversal
    T value = 421;  // Binary: 110100101
    assert(Bit::st_reverse_low_bits(value, 1) == 1);   //         1
    assert(Bit::st_reverse_low_bits(value, 2) == 2);   //        10
    assert(Bit::st_reverse_low_bits(value, 3) == 5);   //       101
    assert(Bit::st_reverse_low_bits(value, 4) == 10);  //      1010
    assert(Bit::st_reverse_low_bits(value, 5) == 20);  //     10100
    assert(Bit::st_reverse_low_bits(value, 6) == 41);  //    101001
    assert(Bit::st_reverse_low_bits(value, 7) == 82);  //   1010010
    assert(Bit::st_reverse_low_bits(value, 8) == 165); //  10100101
    assert(Bit::st_reverse_low_bits(value, 9) == 331); // 101001011

}


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
