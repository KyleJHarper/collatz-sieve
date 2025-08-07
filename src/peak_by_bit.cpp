#include "collatz/collatz.hpp"
#include <gmpxx.h>
#include "CLI11.hpp"
#include "logging.hpp"
#include "spdlog/common.h"


//
// Peak VI Scan Results
// Save the results of the IV Scanner in this object.
//
class PeakIVScannerResults {
    private:
    std::map<size_t, mpz_class> _results;

    public:
    explicit PeakIVScannerResults() {}

    void set(size_t bit, mpz_class max_initial_value) { _results[bit] = max_initial_value ; }
    const mpz_class& get(size_t bit) const { return _results.at(bit); }
    const std::map<size_t, mpz_class>& get_results() const { return _results; }

    void merge(const PeakIVScannerResults& other_results) {
        for (auto& [key, max_initial_value] : other_results.get_results()) {
            _results[key] = max_initial_value;
        }
    }
};


//
// Peak Initial Value Scanner
// Find the highest initial value for each 2^bit requested.
// Auto-upgrade to mpz_class when upon first encountering an overflow: CollatzSequenceOverflow (or reaching 64 bits).
//
template<IntegralOrMPZClass T>
class PeakIVScanner {
    static_assert(std::integral<T> || std::same_as<T, mpz_class>, "Unsupported type for PeakIVScanner");

    private:
    static constexpr size_t BUFFER_SIZE = 1000;
    T _base_initial_value = 1;
    T _max_allowed_value = 0;
    size_t _start_bit = 0;
    size_t _max_bit = 8;
    std::array<Collatz<T>, BUFFER_SIZE> _collatz;

    public:
    PeakIVScanner() {}
    PeakIVScanner(size_t max_bit) {
        _max_bit = max_bit;
    }
    PeakIVScanner(size_t max_bit, size_t start_bit, T base_initial_value) {
        _max_bit = max_bit;
        _start_bit = start_bit;
        _base_initial_value = base_initial_value;
    }

    PeakIVScannerResults run() {
        PeakIVScannerResults results;
        // Main loop for each bit.
        for (size_t bit = _start_bit; bit <= _max_bit; bit++) {
            // Setup variables based on Integral or mpz_class type.
            if constexpr(std::integral<T>) {
                _max_allowed_value = 1ULL << (bit);
            } else {
                mpz_ui_pow_ui(_max_allowed_value.get_mpz_t(), 2, bit);
            }

            // The main scanning loop.  We will process in batches and stop when we find our value.
            // To achieve parallel execution, we must fill the buffer and allow OMP to loop over it in sections.
            while (true) {
                // Fill the buffer.
                size_t overflow_index = BUFFER_SIZE;
                #pragma omp parallel for reduction(min:overflow_index) schedule(auto) default(none) shared(_collatz, _base_initial_value)
                for(size_t i = 0; i < BUFFER_SIZE; i++) {
                    // We skip even values, so it's always i*2.
                    T my_iv = _base_initial_value + (i * 2);
                    try {
                        _collatz[i].init(my_iv);
                    } catch (const CollatzSequenceOverflow& ex) {
                        overflow_index = i;
                        i = BUFFER_SIZE;  // Cannot 'break' inside OMP loops.  Set i to BUFFER_SIZE to short-circuit out.
                    }
                }

                // If we overflowed, we've reached the limit of uint64.  Time to upgrade to GMP.
                // Don't change base value or anything (i.e.: we need to re-run the sequences with GMP support.)
                // We also need to stop if we hit bit > 63 for integral types.
                bool promote_test = false;
                if constexpr(std::integral<T>) {
                    if (bit > 63) {
                        logger->warn("Reached 64+ bits and must upgrade to GMP.");
                        promote_test = true;
                    }
                    if (overflow_index < BUFFER_SIZE) {
                        const Collatz<T>& overflow_point = _collatz[overflow_index];
                        logger->warn("Reached overflow when filling buffers, in a sequence for uint64_t with IV: {}.  Upgrading to GMP.", overflow_point.get_initial_value());
                        promote_test = true;
                    }
                }
                if (promote_test) {
                    PeakIVScanner<mpz_class> promoted_test(_max_bit, bit, mpz_class(_base_initial_value));
                    PeakIVScannerResults promoted_results;
                    promoted_results = promoted_test.run();
                    // Merge the results and return them.
                    results.merge(promoted_results);
                    return results;
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
                    const Collatz<T>& failure_point = _collatz[failing_index];
                    if constexpr(std::integral<T>) {
                        logger->debug("Found a max IV of {} for 2^{}", failure_point.get_initial_value(), bit);
                    } else {
                        logger->debug("Found a max IV of {} for 2^{}", failure_point.get_initial_value().get_str(), bit);
                    }
                    results.set(bit, mpz_class(failure_point.get_initial_value() - 1));
                    _base_initial_value = failure_point.get_initial_value();
                    // Leave the while(true).  Move to next bit.
                    break;
                }

                // Didn't find an offender.  Bump the initial value, remembering we skip evens.
                _base_initial_value += BUFFER_SIZE * 2;
            }
        }

        // All done.  Return results.
        return results;
    }

};


int main(int argc, char **argv) {
    init_logger();
    // Process options.
    size_t max_bit;
    bool verbose;
    bool force_mpz;
    CLI::App options("Finds the highest initial value (IV) of a Collatz sequence which stays beneath 2^bit during the sequence.  Starts with uint64_t type and upgrades to GMP (mpz_class) automatically.");
    options.add_option("-b,--bits", max_bit, "Number of bits to test to.")->default_val(8);
    options.add_flag("-m,--mpz", force_mpz, "Use GMP's mpz_class immediately instead of escalating to it.");
    options.add_flag(
        "-v,--verbose"
        , [&](size_t x){if(x>0) {verbose=true; logger->set_level(spdlog::level::debug);}}
        , "Enable verbosity."
    );
    CLI11_PARSE(options, argc, argv);
    logger->debug("Selected options were:");
    logger->debug("  Force MPZ: {}", force_mpz);
    logger->debug("  Max Bit: {}", max_bit);
    logger->debug("  Verbose: {}", verbose);

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
        logger->info("2^{} max IV: {}", bit, max_iv.get_str());
    }

    // Go home.
    return 0;
}
