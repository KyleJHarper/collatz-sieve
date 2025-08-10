#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <gmp.h>
#include <gmpxx.h>


//
// Basic wrapper to get spdlog and make formatting consistent.
//
extern std::shared_ptr<spdlog::logger> logger;

// Function to initialize it
void init_logger();


//
// Custom formatter for mpz_class
//
template <>
struct fmt::formatter<mpz_class> : fmt::formatter<std::string> {
    constexpr auto parse(fmt::format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const mpz_class& value, FormatContext& ctx) const {
        // Convert mpz_class to string using get_str()
        return fmt::format_to(ctx.out(), "{}", value.get_str());
    }
};
//
// And for mpf
//
template <>
struct fmt::formatter<mpf_class> : fmt::formatter<std::string> {
    constexpr auto parse(fmt::format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const mpf_class& value, FormatContext& ctx) const {
        // Use long type for base and precision
        mp_exp_t exp = 10;
        std::string str = value.get_str(exp);
        return fmt::format_to(ctx.out(), "{}", str);
    }
};