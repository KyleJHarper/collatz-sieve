#include <gmp.h>
#include <gmpxx.h>
#include <CLI/CLI.hpp>
#include "collatz/concepts.hpp"
#include "collatz/logging.hpp"
#include "collatz/collatz.hpp"
#include "collatz/binary_tree_math.hpp"
#include "collatz/progress.hpp"
#include "collatz/peak_by_bit_gpu_interface.hpp"

#ifdef HAVE_CUDA
#include <cuda_runtime_api.h>
bool can_use_gpu() {
    int count = 0;
    if (cudaGetDeviceCount(&count) != cudaSuccess || count == 0) {
        return false;
    }
    return true;
}
#else
bool can_use_gpu() { return false; }
#endif



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


// Forward declare a "no-op" GPU runner for GMP
struct NullGPURunner {
    template<typename... Args>
    void compute_collatz_peaks(Args&&...) {}
};



//
// Peak Initial Value Scanner
// Find the highest initial value for each 2^bit requested.
// Auto-upgrade to mpz_class when upon first encountering an overflow: CollatzSequenceOverflow (or reaching 64 bits).
//
template<AnySupportedIntegral T>
class PeakIVScanner {
    private:
    static constexpr size_t BUFFER_SIZE = 1ULL << 24;
    T _base_initial_value = 1;
    T _max_allowed_value = 0;
    size_t _start_bit = 0;
    size_t _max_bit = 8;
    std::vector<T> _collatz_peaks;


    public:
    PeakIVScanner() {}
    PeakIVScanner(size_t max_bit) {
        _max_bit = max_bit;
    }
    PeakIVScanner(size_t max_bit, size_t start_bit, T base_initial_value = 0) {
        _max_bit = max_bit;
        _start_bit = start_bit;
        size_t selected_initial_bit = _start_bit;
        size_t max_known_bit = CollatzConstants::get_max_bits_for_max_initial_value_by_type<T>();
        if (start_bit > max_known_bit) {
            logger->warn("You started a new PeakIVScanner with a start bit ({}) that doesn't have a known base initial value.  It will attempt to start start at max known bit: {}", start_bit, max_known_bit);
            selected_initial_bit = max_known_bit;
        }
        if constexpr(GMPIntegral<T>) {
            Int128::uint128_to_mpz(CollatzConstants::get_max_initial_value_by_bit<uint128_t>(selected_initial_bit), _base_initial_value);
        } else  {
            _base_initial_value = CollatzConstants::get_max_initial_value_by_bit<T>(selected_initial_bit);
        }
        if (_base_initial_value < base_initial_value) {
            logger->warn("The base initial value you passed in ({}) is higher than the default we know of ({}).  Using yours instead.", base_initial_value, _base_initial_value);
            _base_initial_value = base_initial_value;
        }
    }



    //
    // Find Max IV for Bit
    // Finds the maximum initial value for 2^bit.  Same behavior as the GPU version.
    //
    inline void find_max_iv_for_bit(int& failing_index, int& overflow_index, T* progress_ptr) {
        // Loop until we get a failing index or overflow index.
        while(true) {
            // Load up the buffer.
            static thread_local T my_iv;
            #pragma omp parallel for reduction(min:overflow_index) schedule(auto) default(none) shared(_collatz_peaks, _base_initial_value)
            for(size_t i = 0; i < BUFFER_SIZE; i++) {
                my_iv = _base_initial_value + i;
                try {
                    Collatz<T>::st_get_peak(my_iv, _collatz_peaks[i], true);
                } catch (const CollatzSequenceOverflow& ex) {
                    overflow_index = i;
                    i = BUFFER_SIZE;  // Cannot 'break' inside OMP loops.  Set i to BUFFER_SIZE to short-circuit out.
                }
            }

            // Reduce into the failing index.
            #pragma omp parallel for reduction(min:failing_index) schedule(auto) default(none) shared(_collatz_peaks, _max_allowed_value)
            for (size_t i = 0; i < BUFFER_SIZE; i++) {
                if (_collatz_peaks[i] > _max_allowed_value) {
                    failing_index = i;
                    i = BUFFER_SIZE;  // Cannot 'break' inside OMP loops.  Set i to BUFFER_SIZE to short-circuit out.
                }
            }

            // If either indexes were set, leave.
            if (failing_index < INT_MAX || overflow_index < INT_MAX) {
                break;
            }

            // Nothing found.  Bump the base initial value and continue.
            _base_initial_value += BUFFER_SIZE;
            *progress_ptr = _base_initial_value;
        }
    }


    //
    // Run
    // Runs the scanner and returns results.  Tries to use a GPU if available.
    //
    PeakIVScannerResults run(bool use_table = false) {
        // Setup the vector for result storage.  Reserve the space to avoid realloc() messing with the cuda memcpy later.
        _collatz_peaks.clear();
        _collatz_peaks.shrink_to_fit();
        _collatz_peaks.reserve(BUFFER_SIZE);

        // Setup the failing_index and overflow_index, which are checked in our main loop.  Add pointers for GPU too.
        T* unified_base_initial_value_ptr = nullptr;
        int* unified_overflow_index_ptr = nullptr;
        int* unified_failing_index_ptr = nullptr;

        // Flags.
        bool has_gpu = can_use_gpu();
        bool used_gpu = false;
        bool promote_test = false;

        // Unify memory for the host-device connection if needed.  Juggle between cuda and host alloc.
        // Base Initial Value is a private member, but we need it sync'd for Progress() to track.
#ifdef HAVE_CUDA
        if (has_gpu) {
            cudaMallocManaged(&unified_base_initial_value_ptr, sizeof(T));
            cudaMallocManaged(&unified_overflow_index_ptr, sizeof(int));
            cudaMallocManaged(&unified_failing_index_ptr, sizeof(int));
        } else {
            unified_base_initial_value_ptr = (T*) std::malloc(sizeof(T));
            unified_failing_index_ptr = (int*) std::malloc(sizeof(int));
            unified_overflow_index_ptr = (int*) std::malloc(sizeof(int));
        }
#else
        unified_base_initial_value_ptr = (T*) std::malloc(sizeof(T));
        unified_failing_index_ptr = (int*) std::malloc(sizeof(int));
        unified_overflow_index_ptr = (int*) std::malloc(sizeof(int));
#endif
        *unified_base_initial_value_ptr = _base_initial_value;
        *unified_overflow_index_ptr = INT_MAX;
        *unified_failing_index_ptr = INT_MAX;

        // Create a GPU runner, in case the GPU is leveraged.
        CollatzPeakRunner<T>* gpu_runner = nullptr;
        if constexpr(FixedWidthIntegral<T>) {
            if (has_gpu) {
                gpu_runner = create_runner(
                    _collatz_peaks.data()
                    , BUFFER_SIZE
                    , unified_base_initial_value_ptr
                    , unified_overflow_index_ptr
                    , unified_failing_index_ptr
                );
            }
        }

        // Create the results hash for tracking.
        PeakIVScannerResults results;

        // When the user wants precomputed values, use the table.
        if (use_table) { logger->debug("Using precomputed table where possible."); }
        logger->debug("Starting with bit {} and base initial value of {}", _start_bit, _base_initial_value);

        // Establish progress tracking.
        Progress progress(std::filesystem::current_path().string() + "/peak_by_bit.progress");
        progress.monitor(unified_base_initial_value_ptr, 2);

        // Main loop for each bit.
        for (size_t bit = _start_bit; bit <= _max_bit; bit++) {
            // If we're using the precomputed table, send it.
            if constexpr(FixedWidthIntegral<T>) {
                if (use_table && bit < CollatzConstants::get_max_bits_for_max_initial_value_by_type<T>()) {
                    T max_iv = CollatzConstants::get_max_initial_value_by_bit<T>(bit);
                    logger->debug("Found a max IV of {} for 2^{}", max_iv, bit);
                    if constexpr(sizeof(T) * 8 == 128) {
                        _base_initial_value = max_iv;
                        results.set(bit, Int128::uint128_to_mpz(max_iv));
                    } else {
                        _base_initial_value = max_iv;
                        results.set(bit, mpz_class(max_iv));
                    }
                    continue;
                }
            }

            // Calculate the max allowed value.  It will be -1 since unisgned values start at 0.
            if constexpr(FixedWidthIntegral<T>) {
                _max_allowed_value = T(1) << bit;
            } else {
                mpz_ui_pow_ui(_max_allowed_value.get_mpz_t(), 2, bit);
            }
            _max_allowed_value -= 1;
            logger->debug("Looking for max_allowed_value of 2^{} - 1 == {}, starting with base_initial_value of {}", bit, _max_allowed_value, _base_initial_value);

            // Reset indexes.
            *unified_overflow_index_ptr = INT_MAX;
            *unified_failing_index_ptr = INT_MAX;

            // Find the first failing index.  Use the GPU if we can, otherwise use the CPU.
            used_gpu = false;
            if constexpr(FixedWidthIntegral<T>) {
                if (has_gpu) {
                    // Find the peak via a kernel which will do all the work.
                    find_max_iv_for_bit_gpu(gpu_runner, bit);
                    _base_initial_value = *unified_base_initial_value_ptr;
                    used_gpu = true;
                }
            }
            if (used_gpu == false) {
                find_max_iv_for_bit(*unified_failing_index_ptr, *unified_overflow_index_ptr, unified_base_initial_value_ptr);
            }

            // Check for overflows with 64 and 128 bit integrals.
            promote_test = false;
            if constexpr (FixedWidthIntegral<T>) {
                if constexpr (sizeof(T) * 8 <= 64) {
                    // Working with 64-bit types under the hood.
                    if (bit > 63) {
                        logger->warn("Reached 64+ bits and must upgrade to uint128_t.");
                        promote_test = true;
                    }
                    if (*unified_overflow_index_ptr < INT_MAX) {
                        T overflow_initial_value = _base_initial_value + *unified_overflow_index_ptr;
                        logger->warn("Reached overflow when filling buffers, in a sequence for uint64_t with IV: {}.  Upgrading to uint128_t.", overflow_initial_value);
                        promote_test = true;
                    }
                } else if constexpr (sizeof(T) * 8 == 128) {
                    // Working with 128-bit type.
                    if (bit > 127) {
                        logger->warn("Reached 128+ bits and must upgrade to GMP.");
                        promote_test = true;
                    }
                    if (*unified_overflow_index_ptr < INT_MAX) {
                        T overflow_initial_value= _base_initial_value + *unified_overflow_index_ptr;
                        logger->warn("Reached overflow when filling buffers, in a sequence for uint128_t with IV: {}.  Upgrading to GMP.", overflow_initial_value);
                        promote_test = true;
                    }
                }
            }

            // Promote if needed.
            if (promote_test) {
                progress.join();
                PeakIVScannerResults promoted_results;
                if constexpr(FixedWidthIntegral<T> && sizeof(T) * 8 <= 64) {
                    PeakIVScanner<uint128_t> promoted_test(_max_bit, bit, uint128_t(_base_initial_value));
                    promoted_results = promoted_test.run(use_table);
                } else if constexpr(FixedWidthIntegral<T> && sizeof(T) * 8 == 128) {
                    PeakIVScanner<mpz_class> promoted_test(_max_bit, bit, Int128::uint128_to_mpz(_base_initial_value));
                    promoted_results = promoted_test.run(use_table);
                }
                // Merge the results and return them.
                results.merge(promoted_results);
                progress.join();
#ifdef HAVE_CUDA
                if (has_gpu) {
                    cudaFree(unified_base_initial_value_ptr);
                    cudaFree(unified_failing_index_ptr);
                    cudaFree(unified_overflow_index_ptr);
                } else {
                    std::free(unified_base_initial_value_ptr);
                    std::free(unified_failing_index_ptr);
                    std::free(unified_overflow_index_ptr);
                }
#else
                std::free(unified_base_initial_value_ptr);
                std::free(unified_failing_index_ptr);
                std::free(unified_overflow_index_ptr);
#endif
                return results;
            }

            // We must have found a failing index.  Let's log it and update tracking.
            T failure_initial_value = _base_initial_value + *unified_failing_index_ptr;
            T max_iv = failure_initial_value - 1;
            logger->debug("Found a peak over allowed at index {}.  Next value ({}) hit a peak of {}.", *unified_failing_index_ptr, failure_initial_value, _collatz_peaks[*unified_failing_index_ptr]);
            logger->debug("Found a max IV of {} for 2^{}", max_iv, bit);
            if constexpr(Int128Integral<T>) {
                results.set(bit, Int128::uint128_to_mpz(max_iv));
            } else {
                results.set(bit, mpz_class(max_iv));
            }
            _base_initial_value += *unified_failing_index_ptr;
            *unified_base_initial_value_ptr = _base_initial_value;
        }

        // All done.  Return results.
        progress.join();
#ifdef HAVE_CUDA
        if (has_gpu) {
            cudaFree(unified_base_initial_value_ptr);
            cudaFree(unified_failing_index_ptr);
            cudaFree(unified_overflow_index_ptr);
        } else {
            std::free(unified_base_initial_value_ptr);
            std::free(unified_failing_index_ptr);
            std::free(unified_overflow_index_ptr);
        }
#else
        std::free(unified_base_initial_value_ptr);
        std::free(unified_failing_index_ptr);
        std::free(unified_overflow_index_ptr);
#endif
        return results;
    }

};


int main(int argc, char **argv) {
    init_logger();
    // Process options.
    size_t start_bit;
    size_t max_bit;
    bool verbose = false;
    bool force_mpz = false;
    bool force_i128 = false;
    bool use_table = false;
    bool array_output = false;
    std::string starting_value_s;
    uint128_t starting_value = 0;
    CLI::App options("Finds the highest initial value (IV) of a Collatz sequence which stays beneath 2^bit during the sequence.  Starts with uint64_t type and upgrades to GMP (mpz_class) automatically.");
    options.add_flag("-a,--array-output", array_output, "Emit the results as an array for easy copy/paste.");
    options.add_option("-b,--bits", max_bit, "Number of bits to test to.")->default_val(8);
    options.add_flag("-i,--int128", force_i128, "Use 128-bit native immediately instead of escalating to it.");
    options.add_flag("-m,--mpz", force_mpz, "Use GMP's mpz_class immediately instead of escalating to it.");
    options.add_option("-s,--start", start_bit, "Bit numer to start at.")->default_val(0);
    options.add_option("-S,--starting-value", starting_value_s, "Use this starting value, if larger than bit mapped value (for continuation).")->default_val("0");
    options.add_flag(
        "-v,--verbose"
        , [&](size_t x){if(x>0) {verbose=true; logger->set_level(spdlog::level::debug);}}
        , "Enable verbosity."
    );
    options.add_flag("-t,--table", use_table, "Use the precomputed table when possible.");
    CLI11_PARSE(options, argc, argv);
    starting_value = Int128::str_to_uint128(starting_value_s);
    logger->debug("Selected options were:");
    logger->debug("  Force int128: {}", force_i128);
    logger->debug("  Force MPZ: {}", force_mpz);
    logger->debug("  Start Bit: {}", start_bit);
    logger->debug("  Max Bit: {}", max_bit);
    logger->debug("  Starting Value: {}", starting_value);
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
    if (can_use_gpu()) {
        logger->info("GPU was detected.  Will use it.");
    } else {
        logger->info("GPU not found.  Falling back to CPU processing.");
    }
    PeakIVScannerResults results;
    if (force_mpz) {
        PeakIVScanner<mpz_class> test(max_bit, start_bit, Int128::uint128_to_mpz(starting_value));
        results = test.run(use_table);
    } else if (force_i128) {
        PeakIVScanner<uint128_t> test(max_bit, start_bit, starting_value);
        results = test.run(use_table);
    } else {
        PeakIVScanner<uint64_t> test(max_bit, start_bit, uint64_t(starting_value));
        results = test.run(use_table);
    }

    // Print results.
    mpz_class twos_power;
    for (auto& [bit, max_iv] : results.get_results()) {
        mpz_ui_pow_ui(twos_power.get_mpz_t(), 2, bit);
        logger->info("2^{} ({}) has max IV: {:'} (max full level: {})", bit, twos_power, max_iv, BinaryTreeMath<mpz_class>::st_max_full_level_at_node_value(max_iv));
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

    // Sanity check.
    bool coherent = true;
    mpz_class comparison = 0;
    for (auto& [bit, max_iv] : results.get_results()) {
        comparison = Int128::uint128_to_mpz(CollatzConstants::get_max_initial_value_by_bit<uint128_t>(bit));
        if (max_iv != comparison) {
            coherent = false;
            logger->warn("For 2^{} the calculated IV was {} but our records show {}.", bit, max_iv, comparison);
        }
    }
    if (!coherent) {
        logger->warn("Coherency test failed!  See warnings above.");
    } else {
        logger->info("Coherency passed.  Computed values match known values.");
    }

    // Go home.
    return 0;
}
