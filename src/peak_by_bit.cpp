#include "collatz/peak_iv_scanner.hpp"
#include <iostream>
#include <gmpxx.h>
#include "include/CLI11.hpp"


void print_options(size_t& max_bit, bool& verbose, bool& force_mpz) {
    std::cerr << "Selected options were:" << std::endl;
    std::cerr << "  Force MPZ: " << (force_mpz ? "true" : "false") << std::endl;
    std::cerr << "  Max Bit: " << max_bit << std::endl;
    std::cerr << "  Verbose: " << (verbose ? "true" : "false") << std::endl;
}


int main(int argc, char **argv) {
    // Process options.
    size_t max_bit;
    bool verbose;
    bool force_mpz;
    CLI::App options("Finds the highest initial value (IV) of a Collatz sequence which stays beneath 2^bit during the sequence.  Starts with uint64_t type and upgrades to GMP (mpz_class) automatically.");
    options.add_option("-b,--bits", max_bit, "Number of bits to test to.")->default_val(8);
    options.add_flag("-m,--mpz", force_mpz, "Use GMP's mpz_class immediately instead of escalating to it.");
    options.add_flag("-v,--verbose", verbose, "Enable verbosity.");
    CLI11_PARSE(options, argc, argv);

    // If verbose, show options.
    if (verbose) {
        print_options(max_bit, verbose, force_mpz);
        PeakIVScanner<mpz_class>::verbose = true;
        PeakIVScanner<uint64_t>::verbose = true;
    }

    // Build the tester and run it.
    PeakIVScannerResults results;
    if (force_mpz) {
        PeakIVScanner<mpz_class> test(max_bit);
        results = test.run();
    } else {
        PeakIVScanner<uint64_t> test(max_bit);
        results = test.run();
    }

    for (auto& [bit, max_iv] : results.get_results()) {
        std::cout << "2^" << bit << " max IV: " << max_iv << "." << std::endl;
    }

    // Go home.
    return 0;
}
