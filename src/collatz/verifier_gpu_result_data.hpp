#pragma once
#include "abi.hpp"

#include <cstdint>


/**
* @struct GPUVerifierResultData
* @brief Holds the result data, which is mostly overflow values/indexes for CPU verification using a larger type (e.g. mpz_class).
*/
struct alignas(ABI::CACHE_LINE_SIZE) GPUVerifierResultData {
    static constexpr uint32_t MAX_OVERFLOW = 4096;   ///< Maximum size of the overflow table.  Keeping this small reduces PCIe traffic.
    uint32_t overflow_count = 0;                     ///< How many items overflowed beyond the GPU's capability.
    uint32_t overflow_indexes[MAX_OVERFLOW] = {};    ///< The indexes which overflowed, which are available in the host memory too.
    bool overflow_exceeded = false;                  ///< If the GPU ran out of slots, the CPU should verify the whole block.

    /// @brief Reset the counters so this can be reused.
    void reset() {
        overflow_count = 0;
        overflow_exceeded = false;
    }
};
