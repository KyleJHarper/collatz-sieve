#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <gmp.h>
#include <gmpxx.h>
#include <string_view>
#include "typedefs.hpp"
#include "int128.hpp"
#include <cctype>
#include <algorithm>


/// @brief Basic wrapper to get spdlog and make formatting consistent.
extern std::shared_ptr<spdlog::logger> logger;

/// @brief Forward declare an initializer.  Add an overload for string vs char*.
void init_logger(const std::string);
void init_logger(const char*);





/**
* @struct fmt::formatter<int128_t>
* @brief Converts `int128_t` types to string for logging.
*/
template <>
struct fmt::formatter<int128_t> : fmt::formatter<std::string> {
    auto format(const int128_t& value, format_context& ctx) const {
        return fmt::formatter<std::string>::format(Int128::int128_to_string(value), ctx);
    }
};



/**
* @struct fmt::formatter<uint128_t>
* @brief Converts `uint128_t` types to string for logging.
*/
template <>
struct fmt::formatter<uint128_t> : fmt::formatter<std::string> {
    auto format(const uint128_t& value, format_context& ctx) const {
        return fmt::formatter<std::string>::format(Int128::uint128_to_string(value), ctx);
    }
};



/**
* @struct fmt::formatter<mpz_class>
* @brief Converts `mpz_class` types to string for logging.
* @warning This is AI garbage as a stop-gap.  It's fragile and needs refactoring.
*/
template <>
struct fmt::formatter<mpz_class> {
    // store the raw spec (the characters between ':' and '}')
    std::string_view spec_;

    constexpr auto parse(fmt::format_parse_context& ctx) {
        auto it = ctx.begin();
        auto end = ctx.end();
        auto start = it;
        // collect everything up to '}' (don't validate here)
        while (it != end && *it != '}') ++it;
        spec_ = std::string_view(&*start, static_cast<size_t>(it - start));
        return it;
    }

    template <typename FormatContext>
    auto format(const mpz_class& value, FormatContext& ctx) const {
        // --- Simple parse of relevant flags from spec_ ---
        bool grouping = (spec_.find('\'') != std::string_view::npos);

        // width: first run of digits not after '.' (precision)
        int width = 0;
        bool in_precision = false;
        for (size_t i = 0; i < spec_.size(); ++i) {
            char c = spec_[i];
            if (c == '.') { in_precision = true; continue; }
            if (in_precision) continue;
            if (std::isdigit(static_cast<unsigned char>(c))) {
                int val = 0;
                size_t j = i;
                while (j < spec_.size() && std::isdigit(static_cast<unsigned char>(spec_[j]))) {
                    val = val * 10 + (spec_[j] - '0');
                    ++j;
                }
                width = val;
                break;
            }
        }

        // alignment and fill (simple heuristic: if a '<', '>' or '^' exists, previous char is fill)
        char align = '>'; // default numeric alignment: right
        char fill = ' ';
        for (size_t i = 0; i < spec_.size(); ++i) {
            char c = spec_[i];
            if (c == '<' || c == '>' || c == '^') {
                align = c;
                if (i >= 1) {
                    char maybe_fill = spec_[i - 1];
                    // treat as fill if it's not a digit or '.' (heuristic)
                    if (!std::isdigit(static_cast<unsigned char>(maybe_fill)) && maybe_fill != '.' && maybe_fill != ':')
                        fill = maybe_fill;
                }
                break;
            }
        }

        // type char: last alphabetic char in spec_ (if any)
        char type = '\0';
        for (auto it = spec_.rbegin(); it != spec_.rend(); ++it) {
            if (std::isalpha(static_cast<unsigned char>(*it))) { type = *it; break; }
        }

        // --- Convert mpz_class to a digits string for requested base ---
        bool neg = value < 0;
        mpz_class absval = neg ? -value : value;
        std::string digits;
        if (type == 'x' || type == 'X') digits = absval.get_str(16);
        else if (type == 'o') digits = absval.get_str(8);
        else if (type == 'b') digits = absval.get_str(2);
        else digits = absval.get_str(10);

        if (type == 'X') // uppercase hex
            std::transform(digits.begin(), digits.end(), digits.begin(), [](unsigned char c){ return std::toupper(c); });

        // --- Grouping (only apply sensible grouping for decimal) ---
        std::string core;
        if ((type == '\0' || type == 'd' || type == 'n') && grouping) {
            // insert commas every 3 digits from the right
            int n = (int)digits.size();
            int first = n % 3;
            if (first == 0) first = 3;
            int i = 0;
            std::string out;
            for (; i < first && i < n; ++i) out.push_back(digits[i]);
            for (; i < n; i += 3) {
                if (!out.empty()) out.push_back(',');
                out.append(digits.substr(i, 3));
            }
            core = std::move(out);
        } else {
            core = digits;
        }

        if (neg) core.insert(core.begin(), '-');

        // --- width/alignment/fill (basic) ---
        if (width > (int)core.size()) {
            int pad = width - (int)core.size();
            if (align == '<') {
                core.append(pad, fill);
            } else if (align == '^') {
                int left = pad / 2;
                int right = pad - left;
                core.insert(core.begin(), left, fill);
                core.append(right, fill);
            } else { // '>' or default
                core.insert(core.begin(), pad, fill);
            }
        }

        return fmt::format_to(ctx.out(), "{}", core);
    }
};



/**
* @struct fmt::formatter<mpf_class>
* @brief Converts `mpf_class` types to string for logging.
* @warning This is AI garbage as a stop-gap.  It's fragile and needs refactoring.
*/
template <>
struct fmt::formatter<mpf_class> {
    std::string_view spec_;
    int precision_ = -1; // -1 == default
    char type_ = 'g';    // default: general

    constexpr auto parse(fmt::format_parse_context& ctx) {
        // capture whole spec first
        auto it = ctx.begin();
        auto end = ctx.end();
        auto start = it;
        while (it != end && *it != '}') ++it;
        spec_ = std::string_view(&*start, static_cast<size_t>(it - start));

        // extract precision (.N) and type char (f/e/g)
        for (size_t i = 0; i < spec_.size(); ++i) {
            char c = spec_[i];
            if (c == '.') {
                // parse precision digits
                int v = 0;
                size_t j = i + 1;
                bool has = false;
                while (j < spec_.size() && std::isdigit(static_cast<unsigned char>(spec_[j]))) {
                    has = true;
                    v = v * 10 + (spec_[j] - '0');
                    ++j;
                }
                if (has) precision_ = v;
            }
        }
        // last alphabetic char is type
        for (auto it2 = spec_.rbegin(); it2 != spec_.rend(); ++it2) {
            if (std::isalpha(static_cast<unsigned char>(*it2))) { type_ = *it2; break; }
        }
        return it;
    }

    template <typename FormatContext>
    auto format(const mpf_class& value, FormatContext& ctx) const {
        // pick significant digits for GMP's get_str
        int sig_digits = (precision_ >= 0) ? (precision_ + 1) : 6;

        mp_exp_t exp;
        std::string mant = value.get_str(exp, 10, sig_digits);

        std::string result;
        switch (type_) {
            case 'f': case 'F': {
                if (exp <= 0) {
                    result = "0." + std::string(-exp, '0') + mant;
                } else if (static_cast<size_t>(exp) >= mant.size()) {
                    result = mant + std::string(exp - mant.size(), '0');
                    if (precision_ > 0) result += "." + std::string(precision_, '0');
                } else {
                    result = mant.substr(0, exp) + "." + mant.substr(exp);
                }
                break;
            }
            case 'e': case 'E': {
                result = mant.substr(0, 1);
                if (mant.size() > 1) result += "." + mant.substr(1);
                result += (type_ == 'E' ? "E" : "e");
                result += (exp - 1 >= 0 ? "+" : "");
                result += std::to_string(exp - 1);
                break;
            }
            case 'g': case 'G':
            default: {
                if (exp - 1 >= 6 || exp - 1 <= -4) {
                    // scientific
                    result = mant.substr(0, 1);
                    if (mant.size() > 1) result += "." + mant.substr(1);
                    result += (type_ == 'G' ? "E" : "e");
                    result += (exp - 1 >= 0 ? "+" : "");
                    result += std::to_string(exp - 1);
                } else {
                    if (exp <= 0) {
                        result = "0." + std::string(-exp, '0') + mant;
                    } else if (static_cast<size_t>(exp) >= mant.size()) {
                        result = mant + std::string(exp - mant.size(), '0');
                        if (precision_ > 0) result += "." + std::string(precision_, '0');
                    } else {
                        result = mant.substr(0, exp) + "." + mant.substr(exp);
                    }
                }
                break;
            }
        }

        // If precision specified for fixed-style types, enforce number of digits after dot
        if (precision_ >= 0 && type_ != 'g' && type_ != 'G') {
            auto dot = result.find('.');
            if (dot == std::string::npos) {
                if (precision_ > 0) result += "." + std::string(precision_, '0');
            } else {
                size_t after = result.size() - dot - 1;
                if (after < static_cast<size_t>(precision_)) result += std::string(precision_ - after, '0');
                else if (after > static_cast<size_t>(precision_)) result.resize(dot + 1 + precision_);
            }
        }

        // grouping (apostrophe) detection and simple insertion (only for decimal-style outputs)
        bool grouping = (spec_.find('\'') != std::string_view::npos);
        if (grouping) {
            // find integer part and fractional part
            auto dot = result.find('.');
            std::string intpart = (dot == std::string::npos) ? result : result.substr(0, dot);
            std::string fracpart = (dot == std::string::npos) ? std::string() : result.substr(dot); // include dot
            bool neg = false;
            if (!intpart.empty() && intpart[0] == '-') { neg = true; intpart.erase(intpart.begin()); }
            // group integer digits
            std::string out;
            int n = (int)intpart.size();
            int first = n % 3;
            if (first == 0) first = 3;
            int i = 0;
            for (; i < first && i < n; ++i) out.push_back(intpart[i]);
            for (; i < n; i += 3) {
                if (!out.empty()) out.push_back(',');
                out.append(intpart.substr(i, 3));
            }
            if (neg) out.insert(out.begin(), '-');
            result = out + fracpart;
        }

        // Apply width/alignment/fill (basic) by reusing the same small logic as integer formatter.
        // width/alignment parsing (reuse small logic)
        int width = 0;
        char align = '>'; char fill = ' ';
        bool in_precision_flag = false;
        for (size_t i = 0; i < spec_.size(); ++i) {
            char c = spec_[i];
            if (c == '.') { in_precision_flag = true; continue; }
            if (in_precision_flag) continue;
            if (std::isdigit(static_cast<unsigned char>(c))) {
                int val = 0; size_t j = i;
                while (j < spec_.size() && std::isdigit(static_cast<unsigned char>(spec_[j]))) { val = val*10 + (spec_[j]-'0'); ++j; }
                width = val;
                break;
            }
        }
        for (size_t i = 0; i < spec_.size(); ++i) {
            char c = spec_[i];
            if (c == '<' || c == '>' || c == '^') {
                align = c;
                if (i >= 1) {
                    char maybe_fill = spec_[i-1];
                    if (!std::isdigit(static_cast<unsigned char>(maybe_fill)) && maybe_fill != '.' && maybe_fill != ':')
                        fill = maybe_fill;
                }
                break;
            }
        }

        if (width > (int)result.size()) {
            int pad = width - (int)result.size();
            if (align == '<') result.append(pad, fill);
            else if (align == '^') {
                int left = pad / 2;
                int right = pad - left;
                result.insert(result.begin(), left, fill);
                result.append(right, fill);
            } else result.insert(result.begin(), pad, fill);
        }

        return fmt::format_to(ctx.out(), "{}", result);
    }
};
