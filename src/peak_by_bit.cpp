#include <cstdint>
#include <stdexcept>
#include <unistd.h>
#include <iostream>
#include <stdint.h>
#include <math.h>
#include <gmpxx.h>
#include <array>
#include "collatz/collatz.hpp"
#include "collatz/concepts.hpp"
#include "include/CLI11.hpp"


template<IntegralOrMPZClass T>
class PeakTest {
    private:
    static constexpr size_t BUFFER_SIZE = 1000;
    T _base_initial_value = 1;
    T _max_allowed_value = 0;
    size_t _max_bit = 8;
    std::array<Collatz<T>, BUFFER_SIZE> _collatz;

    public:
    PeakTest() {}
    PeakTest(size_t max_bit) {
        _max_bit = max_bit;
    }

    void run() {
        // Main loop for each bit.
        for (size_t bit = 0; bit <= _max_bit; bit++) {
            // Setup variables based on Integral or mpz_class type.
            if constexpr(std::integral<T>) {
                if (bit < 64) {
                    _max_allowed_value = 2ULL << bit;
                } else {
                    _max_allowed_value = UINT64_MAX;
                }
            } else if constexpr(std::same_as<T, mpz_class>) {
                mpz_ui_pow_ui(_max_allowed_value.get_mpz_t(), 2, bit);
            } else {
                throw std::runtime_error("I don't know how to handle the data type you specified for me.");
            }

            // The main scanning loop.  We will process in batches and stop when we find our value.
            // To achieve parallel execution, we must fill the buffer and allow OMP to loop over it in sections.
            while (true) {
                // Fill the buffer.
                #pragma omp parallel for schedule(auto) default(none) shared(collatz, base_initial_value)
                for(size_t i = 0; i < BUFFER_SIZE; i++) {
                    // We skip even values, so it's always i*2.
                    T my_iv = _base_initial_value + (i * 2);
                    _collatz[i].init(my_iv);
                }

                // Test all values in the buffer, using OMP's reduction to find the minimum offending index, if any.
                size_t failing_index = BUFFER_SIZE;
                #pragma omp parallel for reduction(min:failing_index) schedule(auto) default(none) shared(_collatz, _max_allowed_value)
                for (size_t i = 0; i < BUFFER_SIZE; i++) {
                    if (_collatz[i].get_peak_value() > _max_allowed_value) {
                        failing_index = i;
                        i = BUFFER_SIZE;  // Cannot 'break' inside OMP loops.  Set i to BUFFER_SIZE to short-circuit out.
                    }
                }

                //
            }

        }
    }
};

void print_options(size_t& max_bit, bool& verbose) {
    std::cerr << "Selected options were:" << std::endl;
    std::cerr << "  Max Bit: " << max_bit << std::endl;
    std::cerr << "  Verbose: " << (verbose ? "true" : "false") << std::endl;
}


int main(int argc, char **argv) {
    // Process options.
    size_t max_bit;
    bool verbose;
    CLI::App options("Finds the highest initial value (IV) of a Collatz sequence which stays beneath 2^bit during the sequence.");
    options.add_option("-b,--bits", max_bit, "Number of bits to test to.")->default_val(8);
    options.add_flag("-v,--verbose", verbose, "Enable verbosity.");
    CLI11_PARSE(options, argc, argv);

    // If verbose, show options.
    if (verbose) {
        print_options(max_bit, verbose);
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
    for(size_t bit = 0; bit <= max_bit; bit++) {
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
