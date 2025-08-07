#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

//
// Basic wrapper to get spdlog and make formatting consistent.
//
extern std::shared_ptr<spdlog::logger> logger;

// Function to initialize it
void init_logger();
