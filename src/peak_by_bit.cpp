#include <gmp.h>
#include <gmpxx.h>
#include "CLI.hpp"
#include "collatz/concepts.hpp"
#include "collatz/logging.hpp"
#include "collatz/collatz.hpp"
#include "collatz/binary_tree_math.hpp"


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
template<AnySupportedIntegral T>
class PeakIVScanner {
    private:
    static constexpr size_t BUFFER_SIZE = 100000;
    T _base_initial_value = 1;
    T _max_allowed_value = 0;
    size_t _start_bit = 0;
    size_t _max_bit = 8;
    std::array<T, BUFFER_SIZE> _collatz_peaks;

    public:
    PeakIVScanner() {}
    PeakIVScanner(size_t max_bit) {
        _max_bit = max_bit;
    }
    PeakIVScanner(size_t max_bit, size_t start_bit) {
        _max_bit = max_bit;
        _start_bit = start_bit;
        size_t selected_initial_bit = _start_bit;
        size_t max_known_bit = CollatzConstants::get_max_initial_value_max_bits<T>();
        if (start_bit > max_known_bit) {
            logger->warn("You started a new PeakIVScanner with a start bit ({}) that doesn't have a known base initial value.  It will start at max known bit: {}", start_bit, max_known_bit);
            selected_initial_bit = max_known_bit;
        }
        if constexpr(GMPIntegral<T>) {
            uint128_to_mpz(CollatzConstants::get_max_initial_value_by_bit<uint128_t>(selected_initial_bit), _base_initial_value);
        } else  {
            _base_initial_value = CollatzConstants::get_max_initial_value_by_bit<T>(selected_initial_bit);
        }
    }
    PeakIVScanner(size_t max_bit, size_t start_bit, T base_initial_value) {
        _max_bit = max_bit;
        _start_bit = start_bit;
        _base_initial_value = base_initial_value;
    }

    PeakIVScannerResults run(bool use_table = false) {
        PeakIVScannerResults results;
        if (use_table) { logger->debug("Using precomputed table where possible."); }
        logger->debug("Starting with bit {} and base initial value of {}", _start_bit, _base_initial_value);
        // Main loop for each bit.
        for (size_t bit = _start_bit; bit <= _max_bit; bit++) {
            // If we're using the precomputed table, send it.
            if constexpr(is_builtin_integral_v<T>) {
                if (use_table && bit < CollatzConstants::get_max_initial_value_max_bits<T>()) {
                    T max_iv = CollatzConstants::get_max_initial_value_by_bit<T>(bit);
                    logger->debug("Found a max IV of {} for 2^{}", max_iv, bit);
                    if constexpr(ExtendedIntegral<T>) {
                        _base_initial_value = max_iv;
                        results.set(bit, uint128_to_mpz(max_iv));
                    } else {
                        _base_initial_value = max_iv;
                        results.set(bit, mpz_class(max_iv));
                    }
                    continue;
                }
            }

            // Setup variables based on Integral or mpz_class type.
            if constexpr(BuiltinIntegral<T>) {
                _max_allowed_value = T(1) << bit;
            } else {
                mpz_ui_pow_ui(_max_allowed_value.get_mpz_t(), 2, bit);
            }
            // Subtract one for 0-based uinsigned integer reality.
            _max_allowed_value -= 1;
            logger->debug("Looking for max_allowed_value of 2^{} - 1 == {}, starting with base_initial_value of {}", bit, _max_allowed_value, _base_initial_value);

            // The main scanning loop.  We will process in batches and stop when we find our value.
            // To achieve parallel execution, we must fill the buffer and allow OMP to loop over it in sections.
            while (true) {
                // Fill the buffer.
                size_t overflow_index = BUFFER_SIZE;
                #pragma omp parallel for reduction(min:overflow_index) schedule(auto) default(none) shared(_collatz_peaks, _base_initial_value)
                for(size_t i = 0; i < BUFFER_SIZE; i++) {
                    T my_iv = _base_initial_value + i;
                    try {
                        // Start with N as the peak.
                        _collatz_peaks[i] = my_iv;
                        // Loop throug sequence.
                        Collatz<T>::for_each_sequence_step(my_iv, [&](const T& step) {
                            // Promote peaks.
                            if (step > _collatz_peaks[i]) {
                                _collatz_peaks[i] = step;
                            }
                            // Skip when we hit HWM.
                            return step < my_iv;
                        });
                    } catch (const CollatzSequenceOverflow& ex) {
                        overflow_index = i;
                        i = BUFFER_SIZE;  // Cannot 'break' inside OMP loops.  Set i to BUFFER_SIZE to short-circuit out.
                    }
                }

                // If we overflowed, we've reached the limit of uint64.  Time to upgrade to GMP.
                // Don't change base value or anything (i.e.: we need to re-run the sequences with GMP support.)
                // We also need to stop if we hit bit > 63 for integral types.
                bool promote_test = false;
                if constexpr(NativeIntegral<T>) {
                    if (bit > 63) {
                        logger->warn("Reached 64+ bits and must upgrade to uint128_t.");
                        promote_test = true;
                    }
                    if (overflow_index < BUFFER_SIZE) {
                        T overflow_initial_value = _base_initial_value + overflow_index;
                        logger->warn("Reached overflow when filling buffers, in a sequence for uint64_t with IV: {}.  Upgrading to GMP.", overflow_initial_value);
                        promote_test = true;
                    }
                } else if constexpr(ExtendedIntegral<T>) {
                    if (bit > 127) {
                        logger->warn("Reached 128+ bits and must upgrade to GMP.");
                        promote_test = true;
                    }
                    if (overflow_index < BUFFER_SIZE) {
                        T overflow_initial_value= _base_initial_value + overflow_index;
                        logger->warn("Reached overflow when filling buffers, in a sequence for uint128_t with IV: {}.  Upgrading to GMP.", overflow_initial_value);
                        promote_test = true;
                    }
                }

                // Promote if needed.
                if (promote_test) {
                    PeakIVScannerResults promoted_results;
                    if constexpr(NativeIntegral<T>) {
                        PeakIVScanner<uint128_t> promoted_test(_max_bit, bit, uint128_t(_base_initial_value));
                        promoted_results = promoted_test.run(use_table);
                    } else if constexpr(ExtendedIntegral<T>) {
                        PeakIVScanner<mpz_class> promoted_test(_max_bit, bit, uint128_to_mpz(_base_initial_value));
                        promoted_results = promoted_test.run(use_table);
                    }
                    // Merge the results and return them.
                    results.merge(promoted_results);
                    return results;
                }

                // Test all values in the buffer, using OMP's reduction to find the minimum offending index, if any.
                size_t failing_index = BUFFER_SIZE;
                #pragma omp parallel for reduction(min:failing_index) schedule(auto) default(none) shared(_collatz_peaks, _max_allowed_value)
                for (size_t i = 0; i < BUFFER_SIZE; i++) {
                    if (_collatz_peaks[i] > _max_allowed_value) {
                        failing_index = i;
                        i = BUFFER_SIZE;  // Cannot 'break' inside OMP loops.  Set i to BUFFER_SIZE to short-circuit out.
                    }
                }

                // If failing_index was set lower than buffer_size (above the highest index), we found an offender.
                if (failing_index < BUFFER_SIZE) {
                    T failure_initial_value = _base_initial_value + failing_index;
                    T max_iv = failure_initial_value - 1;
                    logger->debug("Found a peak over allowed at index {}.  Next value ({}) hit a peak of {}.", failing_index, failure_initial_value, _collatz_peaks[failing_index]);
                    logger->debug("Found a max IV of {} for 2^{}", max_iv, bit);
                    if constexpr(ExtendedIntegral<T>) {
                        results.set(bit, uint128_to_mpz(max_iv));
                    } else {
                        results.set(bit, mpz_class(max_iv));
                    }
                    _base_initial_value += failing_index;
                    // Leave the while(true).  Move to next bit.
                    break;
                }

                // Didn't find an offender.  Bump the initial value.
                _base_initial_value += BUFFER_SIZE;
            }
        }

        // All done.  Return results.
        return results;
    }

};


int main(int argc, char **argv) {
    init_logger();
    // Process options.
    size_t start_bit;
    size_t max_bit;
    bool verbose;
    bool force_mpz;
    bool force_i128;
    bool use_table;
    bool array_output;
    CLI::App options("Finds the highest initial value (IV) of a Collatz sequence which stays beneath 2^bit during the sequence.  Starts with uint64_t type and upgrades to GMP (mpz_class) automatically.");
    options.add_flag("-a,--array-output", array_output, "Emit the results as an array for easy copy/paste.");
    options.add_option("-b,--bits", max_bit, "Number of bits to test to.")->default_val(8);
    options.add_flag("-i,--int128", force_i128, "Use 128-bit native immediately instead of escalating to it.");
    options.add_flag("-m,--mpz", force_mpz, "Use GMP's mpz_class immediately instead of escalating to it.");
    options.add_option("-s,--start", start_bit, "Bit numer to start at.")->default_val(0);
    options.add_flag(
        "-v,--verbose"
        , [&](size_t x){if(x>0) {verbose=true; logger->set_level(spdlog::level::debug);}}
        , "Enable verbosity."
    );
    options.add_flag("-t,--table", use_table, "Use the precomputed table when possible.");
    CLI11_PARSE(options, argc, argv);
    logger->debug("Selected options were:");
    logger->debug("  Force int128: {}", force_i128);
    logger->debug("  Force MPZ: {}", force_mpz);
    logger->debug("  Start Bit: {}", start_bit);
    logger->debug("  Max Bit: {}", max_bit);
    logger->debug("  Use Table: {}", use_table);
    logger->debug("  Verbose: {}", verbose);
    if (force_i128 && force_mpz) {
        logger->error("You can't specify both uint128 start and mpz start.");
        exit(1);
    }
    if (start_bit > max_bit) {
        logger->error("You can't set the start bit after the max bit.");
        exit(1);
    }

    // Build the tester and run it.
    PeakIVScannerResults results;
    if (force_mpz) {
        PeakIVScanner<mpz_class> test(max_bit, start_bit);
        results = test.run(use_table);
    } else if (force_i128) {
        PeakIVScanner<uint128_t> test(max_bit, start_bit);
        results = test.run(use_table);
    } else {
        PeakIVScanner<uint64_t> test(max_bit, start_bit);
        results = test.run(use_table);
    }

    // Print results.
    mpz_class twos_power;
    for (auto& [bit, max_iv] : results.get_results()) {
        mpz_ui_pow_ui(twos_power.get_mpz_t(), 2, bit);
        logger->info("2^{} ({}) has max IV: {:'} (max full level: {})", bit, twos_power, max_iv, BinaryTreeMath<mpz_class>::st_max_full_level_at_node(max_iv));
    }

    // Array output/
    if (array_output) {
        logger->info("Array output requested.  Voila:");
        size_t result_count = results.get_results().size();
        std::string type = max_bit > 65 ? "uint128_t" : "uint64_t";
        std::cerr << "constexpr std::array<" << type << ", " << result_count << "> MY_ARRAY = {" << std::endl;
        for (auto& [bit, max_iv] : results.get_results()) {
            if (bit > 64) {
                std::cerr << "    \"" << max_iv << "\"_u128,  // " << bit << std::endl;
            } else {
                std::cerr << "    " << max_iv << ",  // " << bit << std::endl;
            }
        }
        std::cerr << "};" << std::endl;
    }

    // Go home.
    return 0;
}
