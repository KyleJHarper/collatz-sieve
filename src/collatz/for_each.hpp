#pragma once



/// @brief Enum helper for letting callers decide between serial (sequential) and parallel operation.
enum class ForEachPolicy {
    SERIAL = 1,
    PARALLEL = 2,
};



/// @brief Enum helper for letting callers decide whether to stop or continue processing.
enum class ForEachSignal {
    CONTINUE = 1,
    BREAK = 2,
};
