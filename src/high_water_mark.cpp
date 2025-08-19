#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gmpxx.h>
#include <gmp.h>
#include <ctime>
#include "logging.hpp"
#include "CLI11.hpp"


int main(int argc, char **argv) {
    init_logger();
    // Process options.
    size_t decimal_precision;
    size_t end_exponent;
    size_t start_exponent;
    bool show_fg;
    bool verbose;
    float progress_percent;
    CLI::App options("Calculates powers of 2 and 3 (exponents) to simulate the increase and decrease of Collatz sequences, but respects the 'high-water mark' concept where 2^x/3^y must be less than 1.");
    options.add_option("-d,--decimal", decimal_precision, "Decimal precision to use when outputting.")->default_val(14);
    options.add_option("-e,--end", end_exponent, "The exponent of 3 to end with, inclusive.")->default_val(20);
    options.add_flag("-f,--fg", show_fg, "Enable the showing of f-g pattern.  Unsure how useful here.");
    options.add_option("-p,--progress", progress_percent, "Enable process indicator every X%. 0 == disabled.")->default_val(0.0f);
    options.add_option("-s,--start", start_exponent, "The exponent of 3 to start with, inclusive.")->default_val(0);
    options.add_flag(
        "-v,--verbose"
        , [&](size_t x){if(x>0) {verbose=true; logger->set_level(spdlog::level::debug);}}
        , "Enable verbosity."
    );
    CLI11_PARSE(options, argc, argv);
    logger->debug("Selected options were:");
    logger->debug("  Decimal Precision: {}", decimal_precision);
    logger->debug("  End Exponent: {}", end_exponent);
    logger->debug("  Progress Percent: {}", progress_percent);
    logger->debug("  Show F-G Pattern: {}", show_fg);
    logger->debug("  Start Exponent: {}", start_exponent);
    logger->debug("  Verbose: {}", verbose);

    // Do work.
    size_t total = end_exponent - start_exponent;
    size_t done = 0;
    size_t exp_2 = 0;
    mpz_t power_of_3_value_mpz;
    mpz_init(power_of_3_value_mpz);
    mpf_t base_3_mpf;
    mpf_t power_of_3_value_mpf, result_mpf;
    mpf_init(base_3_mpf);
    mpf_init(power_of_3_value_mpf);
    mpf_init(result_mpf);
    mpf_set_ui(base_3_mpf, 3);
    float next_progress_report = progress_percent;
    float current_progress = 0;
    double result_d = 0.0;
    for (size_t exp_3 = start_exponent; exp_3 <= end_exponent; exp_3++) {
        // Progress report.
        done = exp_3 - start_exponent;
        current_progress = 1.0f * done / total;
        if (progress_percent > 0 && next_progress_report < current_progress) {
            time_t now = time(0);
            char *dt = ctime(&now);
            logger->info("Finished {:.2f}% ({}/{}) at {}.", 100.0 * current_progress, done, total, dt);
            next_progress_report += progress_percent;
        }

        // Build the float and integer values at the current power of 3.  We need int for sizeinbase() later.
        mpz_ui_pow_ui(power_of_3_value_mpz, 3, exp_3);
        mpf_set_z(power_of_3_value_mpf, power_of_3_value_mpz);

        // Calculate the power of two with logarithm base(2), floor, and +1.
        // GMP doesn't support logarithm exactly, but mpz_sizeinbase() gives us the floor+1 automatically.
        exp_2 = mpz_sizeinbase(power_of_3_value_mpz, 2);

        // Build the result.
        mpf_div_2exp(result_mpf, power_of_3_value_mpf, exp_2);
        result_d = mpf_get_d(result_mpf);

        // Print the result.
        logger->info("3^{} / 2^{},  {:0.{}f}", exp_3, exp_2, result_d, decimal_precision);
    }

    // Clean up.
    mpf_clear(result_mpf);
    mpf_clear(base_3_mpf);
    mpf_clear(power_of_3_value_mpf);
    mpz_clear(power_of_3_value_mpz);

    // Go home.
    return 0;
}
