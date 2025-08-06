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


//
// Run the Peak Test.  Auto-upgrade to mpz_class when bit exceeds 63.
//
template<IntegralOrMPZClass T>
class PeakTest {
    private:
    static constexpr size_t BUFFER_SIZE = 1000;
    static constexpr size_t UINT64_T_MAX_BIT = 63;
    T _base_initial_value = 1;
    T _max_allowed_value = 0;
    size_t _start_bit = 0;
    size_t _max_bit = 8;
    std::array<Collatz<T>, BUFFER_SIZE> _collatz;

    public:
    PeakTest() {}
    PeakTest(size_t max_bit) {
        _max_bit = max_bit;
    }

    void run() {
        // Main loop for each bit.
        for (size_t bit = _start_bit; bit <= _max_bit; bit++) {
            // Setup variables based on Integral or mpz_class type.
            if constexpr(std::integral<T>) {
                // If we're above the limit of what a 64-bit integral can handle, upgrade and quit.
                if (bit > UINT64_T_MAX_BIT) {
                    std::cerr << "Reached max bit for uint64_t: " << UINT64_T_MAX_BIT << ".  Upgrading to GMP." << std::endl;
                    PeakTest<mpz_class> promoted_test(_max_bit);
                    promoted_test.set_base_initial_value(mpz_class(_base_initial_value));
                    promoted_test.set_start_bit(bit);
                    promoted_test.run();
                    return;
                }
                _max_allowed_value = 1ULL << (bit);
            } else if constexpr(std::same_as<T, mpz_class>) {
                mpz_ui_pow_ui(_max_allowed_value.get_mpz_t(), 2, bit);
            } else {
                throw std::runtime_error("I don't know how to handle the data type you specified for me.");
            }

            // The main scanning loop.  We will process in batches and stop when we find our value.
            // To achieve parallel execution, we must fill the buffer and allow OMP to loop over it in sections.
            while (true) {
                // Fill the buffer.
                #pragma omp parallel for schedule(auto) default(none) shared(_collatz, _base_initial_value)
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

                // If failing_index was set lower than buffer_size (above the highest index), we found an offender.
                if (failing_index < BUFFER_SIZE) {
                    const auto& failure_point = _collatz[failing_index];
                    std::cout
                        << "Initial value " << failure_point.get_initial_value()
                        << " has a step landing on " << failure_point.get_peak_value()
                        << " which would exceed 2^" << bit
                        << " (" << _max_allowed_value << ")."
                        << "  Therefore the largest initial value for 2^" << bit
                        << " is " << (failure_point.get_initial_value() - 1)
                        << "." << std::endl;
                    _base_initial_value = failure_point.get_initial_value();
                    // Leave the while(true).  Move to next bit.
                    break;
                }

                // Didn't find an offender.  Bump the initial value, remembering we skip evens.
                _base_initial_value += BUFFER_SIZE * 2;
            }
        }
    }

    void set_start_bit(size_t value) { _start_bit = value; }
    void set_base_initial_value(T value) { _base_initial_value = value; }
};

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
    }

    // Build the tester and run it.
    if (force_mpz) {
        PeakTest<mpz_class> test(max_bit);
        test.run();
    } else {
        PeakTest<uint64_t> test(max_bit);
        test.run();
    }

    // Go home.
    return 0;
}
