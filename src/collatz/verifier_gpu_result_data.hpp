#pragma once
#include "abi.hpp"

#include <cstdint>

struct alignas(ABI::CACHE_LINE_SIZE) GPUVerifierResultData {
    static constexpr uint32_t MAX_OVERFLOW = 4096;
    uint32_t overflow_count = 0;
    uint32_t overflow_indexes[MAX_OVERFLOW] = {};
    bool overflow_exceeded = false;

    void reset() {
        overflow_count = 0;
        overflow_exceeded = false;
    }
};
