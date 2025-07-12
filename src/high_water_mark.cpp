#include <ctype.h>
#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gmpxx.h>
#include <gmp.h>
#include <ctime>
#include <iostream>


#define OPT_DECIMAL_PRECISION_DEFAULT 14
#define OPT_END_DEFAULT 20
#define OPT_EXPANDED_DEFAULT false
#define OPT_PROGRESS_PERCENT_DEFAULT 0.0f
#define OPT_SHOW_FG_DEFAULT false
#define OPT_START_DEFAULT 0
#define OPT_VERBOSE_DEFAULT false
struct Options {
    uint decimal_precision = OPT_DECIMAL_PRECISION_DEFAULT;
    uint end = OPT_END_DEFAULT;
    bool expanded = OPT_EXPANDED_DEFAULT;
    float progress_percent = OPT_PROGRESS_PERCENT_DEFAULT;
    bool show_fg = OPT_SHOW_FG_DEFAULT;
    uint start = OPT_START_DEFAULT;
    bool verbose = OPT_VERBOSE_DEFAULT;
};
Options options;

void show_help() {
    fprintf(stderr, "Usage: ./high_water_mark [-d #] [-e #] [-f] [-p #] [-s #] [-v]\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    -d  #   Decimal precision for to use.  Default is %d places.\n", OPT_DECIMAL_PRECISION_DEFAULT);
    fprintf(stderr, "    -e  #   The exponent of 3 to end with, inclusive.  Default is: %d.\n", OPT_END_DEFAULT);
    fprintf(stderr, "    -f      Enable showing the f-g pattern.  Default is %s.\n", OPT_SHOW_FG_DEFAULT ? "true" : "false");
    fprintf(stderr, "    -h      Show this help and exit.\n");
    fprintf(stderr, "    -p  #   Enable a progress indicator every X%%.  Default is %f (0 == disabled).\n", OPT_PROGRESS_PERCENT_DEFAULT);
    fprintf(stderr, "    -s  #   The exponent of 3 to start with, inclusive.  Default is %d.\n", OPT_START_DEFAULT);
    fprintf(stderr, "    -v      Enable verbose output and debugging to stderr, if any.\n");

}

int process_options(int argc, char **argv) {
    int c;
    opterr = 0;
    while ((c = getopt(argc, argv, "d:e:fhp:s:v")) != -1) {
        switch (c) {
            case 'd':
                options.decimal_precision = atoi(optarg);
                break;
            case 'e':
                options.end = atoi(optarg);
                break;
            case 'f':
                options.show_fg = true;
                break;
            case 'h':
                show_help();
                exit(0);
                break;
            case 'p':
                options.progress_percent = 1.0f * atoi(optarg) / 100.0f;
                break;
            case 's':
                options.start = atoi(optarg);
                break;
            case 'v':
                options.verbose = true;
                break;
            case '?':
                if (optopt == 'c')
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint (optopt))
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
                return 1;
            default:
                abort ();
        }
    }
    return 0;
}

void print_options() {
    fprintf(stderr, "Selected options were:\n");
    fprintf(stderr, "  Decimal Precision: %d\n", options.decimal_precision);
    fprintf(stderr, "  End: %d\n", options.end);
    fprintf(stderr, "  Progress Percent: %f%%\n", 100.0 * options.progress_percent);
    fprintf(stderr, "  Show F-G Pattern: %s\n", options.show_fg ? "true" : "false");
    fprintf(stderr, "  Start: %d\n", options.start);
    fprintf(stderr, "  Verbose: %s\n", options.verbose ? "true" : "false");
}


int main(int argc, char **argv) {
    // Process options.
    process_options(argc, argv);
    if(options.verbose) {
        print_options();
    }

    // Do work.
    int total = options.end - options.start;
    int done = 0;
    int exp_2 = 0;
    mpz_t power_of_3_value_mpz;
    mpz_init(power_of_3_value_mpz);
    mpf_t base_3_mpf;
    mpf_t power_of_3_value_mpf, result_mpf;
    mpf_inits(base_3_mpf);
    mpf_inits(power_of_3_value_mpf, result_mpf);
    mpf_set_ui(base_3_mpf, 3);
    float next_progress_report = options.progress_percent;
    float current_progress = 0;
    double result_d = 0.0;
    for (uint exp_3 = options.start; exp_3 <= options.end; exp_3++) {
        // Progress report.
        done = exp_3 - options.start;
        current_progress = 1.0f * done / total;
        if (options.progress_percent > 0 && next_progress_report < current_progress) {
            time_t now = time(0);
            char *dt = ctime(&now);
            std::cerr << "Finished " << 100.0 * current_progress << "% (" << done << "/" << total << ") at " << dt;
            next_progress_report += options.progress_percent;
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
        printf("3^%d / 2^%d,  %0.*f\n", exp_3, exp_2, options.decimal_precision, result_d);
    }

    // Clean up.
    mpf_clear(result_mpf);
    mpf_clear(base_3_mpf);
    mpf_clear(power_of_3_value_mpf);
    mpz_clear(power_of_3_value_mpz);

    // Go home.
    return 0;
}


// exp_2 = 0
// total = end - start
// next_progress_report = progress_percent
// for exp_3 in range(start, end + 1):
//     # Progress report.
//     done = exp_3 - start
//     if progress_percent > 0 and next_progress_report < (done / total):
//         print(f"Finished {100 * done / total}% ({done}/{total}) at {datetime.now()}", file=sys.stderr)
//         next_progress_report += progress_percent
//     # Get the integer value of the current power of 3.
//     int_3 = pow(3, exp_3)
//     # The power of two will simply be the log-base2 of the integer, ceiling.
//     exp_2 = math.floor(math.log(int_3, 2)) + 1
//     # We have a 2^x/3^y match now.
//     value = pow(3, exp_3) / pow(2, exp_2)
//     line = f"3^{exp_3} / 2^{exp_2},  {value:0<.{precision}f}"
//     if expand:
//         line += f",  {pow(3, exp_3)} / {pow(2, exp_2)}"
//     print(line)
//     # Show pattern.
//     if show_fg:
//         pattern = []
//         f_done = 0
//         g_done = 0
//         f_count = exp_3
//         g_count = exp_2 - exp_3
//         peak_constant = 0
//         while f_done < f_count or g_done < g_count:
//             # If the next power of 2 would hit HWM, bump 3, but only if we're allowed to.
//             if pow(3, f_done) < pow(2, f_done + g_done + 1) and f_done < f_count:
//                 pattern.append('f')
//                 f_done += 1
//                 peak_constant = (peak_constant * 3 / 2) + 0.5
//             else:
//                 pattern.append('g')
//                 g_done += 1
//                 peak_constant = peak_constant / 2
//         print("  " + '->'.join(pattern))
//         print(f"  Peak constant: {peak_constant}")
