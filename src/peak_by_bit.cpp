#include <cstdint>
#include <unistd.h>
#include <iostream>
#include <stdint.h>
#include <math.h>
#include <gmpxx.h>
#include <array>
#include "collatz/collatz.hpp"


#define OPT_MAX_BIT_DEFAULT 8
#define OPT_VERBOSE_DEFAULT false
struct Options {
    size_t max_bit = OPT_MAX_BIT_DEFAULT;
    bool verbose = OPT_VERBOSE_DEFAULT;
};
Options options;

void show_help() {
    std::cerr << "Finds the highest initial value for a Collatz sequence whose sequence members don't exist 2^BIT size." << std::endl;
    std::cerr << std::endl;
    std::cerr << "Usage: ./peak_by_bit [-m #]" << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "    -m  #   Max bits to find the peak value for.  Default is " << OPT_MAX_BIT_DEFAULT << " bits." << std::endl;
    std::cerr << "    -v      Enable verbose output." << std::endl;
    std::cerr << std::endl;
}

int process_options(int argc, char **argv) {
    int c;
    opterr = 0;
    while ((c = getopt(argc, argv, "hm:v")) != -1) {
        switch (c) {
            case 'h':
                show_help();
                exit(0);
                break;
            case 'm':
                options.max_bit = atoi(optarg);
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
    std::cerr << "Selected options were:" << std::endl;
    std::cerr << "  Max Bit: " << options.max_bit << std::endl;
    std::cerr << "  Verbose: " << options.verbose << std::endl;
}


int main(int argc, char **argv) {
    // Process options.
    process_options(argc, argv);
    if(options.verbose) {
        print_options();
    }

    // Do work.
    // typedef uint64_t my_type;
    typedef mpz_class my_type;
    my_type base_initial_value = 1;
    my_type max_allowed_value = 0;
    // In order to parallelize this, we need to build several buffers to check at once.
    const size_t buffer_size = 1000;
    std::array<Collatz<my_type>, buffer_size> collatz;

    // The per-bit loop cannot be parallelized reasonably.
    // Collatz<my_type>::detect_overflow = true;
    for(size_t bit = 0; bit <= options.max_bit; bit++) {
        // max_allowed_value = std::pow(2, bit);
        // if(bit == 64) {
        //     max_allowed_value = UINT64_MAX;
        // }
        mpz_ui_pow_ui(max_allowed_value.get_mpz_t(), 2, bit);
        while (true) {
            // Generate a buffer to scan, using reduction later.
            #pragma omp parallel for schedule(auto) default(none) shared(collatz, base_initial_value)
            for(size_t i = 0; i < buffer_size; i++) {
                // We skip even values, so it's always i*2.
                my_type my_iv = base_initial_value + (i * 2);
                collatz[i].init(my_iv);
            }

            // Now test all values in the buffer, using reduction to find the minimum.
            size_t failing_index = buffer_size;
            #pragma omp parallel for reduction(min:failing_index) schedule(auto) default(none) shared(collatz, max_allowed_value)
            for (size_t i = 0; i < buffer_size; i++) {
                if (collatz[i].get_peak_value() > max_allowed_value || collatz[i].get_peak_value() >= UINT64_MAX) {
                    failing_index = i;
                    // Calling 'break;' inside OMP is invalid.  Set the threads 'i' to buffer_size so we can short-circuit quit.
                    i = buffer_size;
                }
            }

            // If failing_index was set lower than buffer_size (above the highest index), we found an offender.
            if (failing_index < buffer_size) {
                const auto& failure_point = collatz[failing_index];
                std::cout
                    << "Initial value " << failure_point.get_initial_value()
                    << " has a step landing on " << failure_point.get_peak_value()
                    << " which would exceed 2^" << bit
                    << " (" << max_allowed_value << ")."
                    << "  Therefore the largest initial value for 2^" << bit
                    << " is " << (failure_point.get_initial_value() - 1)
                    << "." << std::endl;
                base_initial_value = failure_point.get_initial_value();
                break;  // Leave the while(true).  Move to next bit.
            }

            // Didn't find it.  Bump the initial value, remembering we skip evens.
            base_initial_value += buffer_size * 2;
        }
    }

    // Go home.
    return 0;
}
