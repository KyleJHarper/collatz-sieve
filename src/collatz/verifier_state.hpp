#pragma once
#include <stdint.h>


/// @brief Enum to help control any verifier state.
enum class VerifierState : uint32_t {
    STOPPING = 1,
    STOPPED = 2,
    RUNNING = 3,
    PAUSED = 4,
};
