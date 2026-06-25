#include <cassert>
#include <cstdint>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"
#include "../collatz/collatz_affine_stride.hpp"



void test_collatz_affine_stride_basics() {
    start_test(__func__);

    // Stride
    assert(AffineStride::MAX_STRIDE == 20);
    AffineStride::Stride stride;
    assert(stride.multiply == 1);
    assert(stride.add == 0);
    assert(stride.bits_required == 0);
    assert(stride.shift == 0);

    end_test();
}


void test_collatz_affine_stride_table() {
    start_test(__func__);

    // Pick any table and ensure it matches expectations.
    using ChosenStride = AffineStride::Table<8>;
    assert(ChosenStride::STRIDE_SIZE == 8);
    assert(ChosenStride::MASK == (1ULL << ChosenStride::STRIDE_SIZE) - 1);

    // Table is built from 0 -> 2^stride_size.
    for (size_t i = 0; i < (1ULL << ChosenStride::STRIDE_SIZE); i++) {
        uint64_t n = i;
        AffineStride::Stride stride;
        for (size_t step_count = 0; step_count < ChosenStride::STRIDE_SIZE; step_count++) {
            if (n & 1) {
                stride.add = ((stride.add << 1) + stride.add) + (uint64_t(1) << stride.shift);
                stride.multiply *= 3;
                stride.shift++;
                n = (3 * n + 1) >> 1;
            } else {
                stride.shift++;
                n >>= 1;
            }
            stride.bits_required = std::bit_width(stride.multiply - 1);
        }
        // Now assert.
        assert(stride.add == ChosenStride::TABLE[i].add);
        assert(stride.multiply == ChosenStride::TABLE[i].multiply);
        assert(stride.bits_required == ChosenStride::TABLE[i].bits_required);
        assert(stride.shift == ChosenStride::TABLE[i].shift);
    }

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_collatz_affine_stride_basics();
    test_collatz_affine_stride_table();
}



int main() {
    std::string name = "CollatzAffineStride";
    preamble(name);
    std::cout << "Need to ensure the ceil(log2(multiply)) of a Stride matches the bits_required\n";
    assert(false);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
