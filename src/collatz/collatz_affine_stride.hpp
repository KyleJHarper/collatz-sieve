#pragma once
#include <concepts>
#include <stdint.h>
#include <stddef.h>
#include <array>


//
// Affine Stride
//
// Since affine maps remain affine under composition, we can build "striding" maps like we do in CollatzAffineMap for any N
// whose least significant bits match the composition which created the map.  When thrown into a table, we get the ability to
// stride "k" steps at a time with a single application of the accelerated map.
//
namespace AffineStride {
    // Make a lightweight, data-only struct for the stride factors.
    constexpr size_t MAX_STRIDE = 20;
    struct Stride {
        uint32_t multiply = 1;  // Peak is all F's, which is ceil(log2(3^20)) == 32 bits.
        uint16_t add = 0;       // Peak is all F's, which is ceil(log2(~3324)) == 12 bits.
        uint8_t f_steps = 0;    // Can hold 255 steps.  More than enough.
        uint8_t shift = 0;      // Can hold 255 steps.  More than enough.
    };
    static_assert(sizeof(Stride) == 8);

    // Make a heavier, data-only struct for longer strides.  Not sure if we'll ever use this.
    constexpr size_t MAX_LONG_STRIDE = 40;
    struct LongStride {
        uint64_t multiply = 1;  // Peak is all F's, which is ceil(log2(3^40)) == 64 bits.
        uint32_t add = 0;       // Peak is all F's, which is ceil(log2(~11057331) ==24 bits.
        uint8_t f_steps = 0;    // Can hold 255 steps.  More than enough.
        uint8_t shift = 0;      // Can hold 255 steps.  More than enough.
    };

    // Compile-time-friendly stride table builder.
    template<typename StrideType, size_t bits>
    constexpr std::array<StrideType, 1ULL << bits> build_stride_table() {
        if constexpr (std::same_as<StrideType, Stride>) {
            static_assert(bits <= MAX_STRIDE, "Stride bits exceed MAX_STRIDE");
        } else if constexpr (std::same_as<StrideType, LongStride>) {
            static_assert(bits <= MAX_LONG_STRIDE, "Stride bits exceed MAX_LONG_STRIDE");
        }

        // Build table.
        const uint64_t table_size = uint64_t(1) << bits;
        std::array<StrideType, 1ULL << bits> stride_table{};

        // With our table preallocated, we can use index-based iteration.  Since we're looking at LSBs, we naturally get our order
        // and can simply loop through them sequentially.
        for (uint64_t i = 0; i < table_size; i++) {
            uint64_t n = i;
            StrideType stride;
            for (size_t step_count = 0; step_count < bits; step_count++) {
                if (n & 1) {
                    stride.add = ((stride.add << 1) + stride.add) + (uint64_t(1) << stride.shift);
                    stride.multiply *= 3;
                    stride.shift++;
                    stride.f_steps++;
                    // Mimic the effects on n now.
                    n = (3 * n + 1) >> 1;
                } else {
                    stride.shift++;
                    // Mimic the effects on n now.
                    n >>= 1;
                }
            }
            stride_table[i] = stride;
        }

        return stride_table;
    }

    // Make a stride table to see how it performs.
    static constexpr size_t STRIDE_SIZE = 8;
    static constexpr uint64_t STRIDE_MASK = (1ULL << STRIDE_SIZE) - 1;
    static constexpr auto STRIDE_TABLE = build_stride_table<Stride, STRIDE_SIZE>();
}
