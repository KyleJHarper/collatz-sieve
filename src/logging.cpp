#include "collatz/logging.hpp"

// Define the global logger instance
std::shared_ptr<spdlog::logger> logger = nullptr;

void init_logger() {
    if (!logger) {
        logger = spdlog::stdout_color_mt("main");
        logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
    }
}
