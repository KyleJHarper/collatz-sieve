#pragma once



/// @brief Enum to help control any verifier state.
enum class VerifierState {
    STOPPING = 1,
    STOPPED = 2,
    RUNNING = 3,
    PAUSED = 4,
};
