#include <filesystem>
#include "collatz/logging.hpp"
#include "spdlog/spdlog.h"


// Define the global logger instance
std::shared_ptr<spdlog::logger> logger = nullptr;

void init_logger() {
    if (!logger) {
        auto cwd = std::filesystem::current_path().string();
        auto logger_name = "combined";
        auto logger_file = "everything.log";

        std::vector<spdlog::sink_ptr> sinks{
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(logger_file),
        };
        logger = std::make_shared<spdlog::logger>(logger_name, begin(sinks), end(sinks));
        logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
        spdlog::set_default_logger(logger);
        spdlog::flush_every(std::chrono::seconds(1));
    }
}
