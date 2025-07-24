#include <unistd.h>
#include <iostream>


#define OPT_MAX_BIT_DEFAULT 16
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
    for(size_t bit = 0; bit <= options.max_bit; bit++) {
        std::cerr << "Processing 2^" << bit << "." << std::endl;
        // We don't need the whole Collatz object.
    }

    // Go home.
    return 0;
}
