#pragma once



/// @brief Enum helper for letting callers decide between serial (sequential) and parallel operation.
enum class ForEachPolicy {
    SERIAL = 1,
    PARALLEL = 2,
};
