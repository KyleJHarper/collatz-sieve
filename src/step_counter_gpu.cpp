#include <cstdint>
#include <filesystem>
#include <gmpxx.h>
#include <stdexcept>
#include "CLI.hpp"
#include "collatz/binary_tree_math.hpp"
#include "collatz/concepts.hpp"
#include "collatz/logging.hpp"
#include "collatz/collatz.hpp"
#include "collatz/progress.hpp"
#include "collatz/step_counter_gpu_interface.hpp"

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



template<AnySupportedIntegral T>
void process_level(T* start_value, T max_value, size_t level, StepResults *results) {
    // Do the work in batches.  Gives us the ability to parallelize better.
    size_t loop_limit = SIZE_MAX;
    size_t loops_needed = 0;
    while(*start_value <= max_value) {
        loops_needed = max_value - (*start_value) + 1;  // +1 because inclusive counting
        if (loops_needed < loop_limit) {
            loop_limit = loops_needed;
        }

        // Do the loop
        #pragma omp parallel default(none) shared(start_value, loop_limit, level, results)
        {
            StepResults local_results;

            #pragma omp for schedule(static)
            for (size_t i = 0; i < loop_limit; i++) {
                tally_t steps = Collatz<T>::st_get_step_count(*start_value + i);
                local_results.add(level, steps);
            }

            // Summarize.
            #pragma omp critical
            results->merge(local_results);
        }

        // Update the start_value.
        *start_value += loop_limit;
    }
}



template<AnySupportedIntegral T>
StepResults run_it(size_t start_bit, size_t max_bit) {
    StepResults results;
    size_t bit_limit = CollatzConstants::get_max_initial_value_max_bits<T>();
    bool has_gpu = can_use_gpu();
    T* unified_start_value_ptr = nullptr;
    T max_value = 0;
    size_t level;

    // Allocate the memory of the shared start value, for tracking purposes (Progress()).
#ifdef HAVE_CUDA
        if (has_gpu) {
            cudaMallocManaged(&unified_start_value_ptr, sizeof(T));
        } else {
            unified_start_value_ptr = (T*) std::malloc(sizeof(T));
        }
#else
        unified_start_value_ptr = (T*) std::malloc(sizeof(T));
#endif

    // Establish progress tracking.
    Progress progress(std::filesystem::current_path().string() + "/step_counter.progress");
    progress.monitor(unified_start_value_ptr, 2);

    // Go bit by bit.
    for (size_t bit = start_bit; bit <= max_bit; bit++) {
        logger->debug("Processing bit {}.", bit);
        // Overflow check.
        if (bit > bit_limit) {
            if constexpr(NativeIntegral<T>) {
                progress.join();
                StepResults other_results = run_it<uint128_t>(bit, max_bit);
                results.merge(other_results);
                return results;
            }
            throw std::out_of_range("This doesn't support over " + to_string_any(bit_limit) + " bits yet for overflow reasons.");
        }

        // Setup
        *unified_start_value_ptr = T(1) << bit;
        max_value = (T(1) << (bit + 1)) - 1;
        level = BinaryTreeMath<T>::st_node_level(*unified_start_value_ptr);
        if (level != BinaryTreeMath<T>::st_node_level(max_value)) {
            throw std::logic_error("Level for start_value and max_value didn't line up.  This is wrong.");
        }

        // Process the level.
        if (can_use_gpu()) {
            process_level_gpu(unified_start_value_ptr, max_value, level, &results);
        } else {
            process_level(unified_start_value_ptr, max_value, level, &results);
        }
    }

    // Return results.
    return results;
}



int main(int argc, char **argv) {
    init_logger();
    // Process options.
    size_t start_bit;
    size_t max_bit;
    bool verbose;
    CLI::App options("Counts steps and emits them.");
    options.add_option("-b,--bits", max_bit, "Number of bits to test to.")->default_val(8);
    options.add_option("-s,--start", start_bit, "Bit numer to start at.")->default_val(0);
    options.add_flag(
        "-v,--verbose"
        , [&](size_t x){if(x>0) {verbose=true; logger->set_level(spdlog::level::debug);}}
        , "Enable verbosity."
    );
    CLI11_PARSE(options, argc, argv);
    logger->debug("Selected options were:");
    logger->debug("  Start Bit: {}", start_bit);
    logger->debug("  Max Bit: {}", max_bit);
    logger->debug("  Verbose: {}", verbose);
    if (start_bit > max_bit) {
        logger->error("You can't set the start bit after the max bit.");
        exit(1);
    }

    // Run it.
    StepResults results;
    if (start_bit < 64) {
        results = run_it<uint64_t>(start_bit, max_bit);
    } else {
        results = run_it<uint128_t>(start_bit, max_bit);
    }

    // Report.
    double avg = 0;
    size_t sum = 0;
    for (auto [level, count] : results.get_by_level_count()) {
        sum = results.get_by_level_sum().find(level)->second;
        avg = 1.0f * sum / count;
        logger->info("Level {} has a count of {} items with {} steps.  ~{} steps/item."
            , level
            , count
            , results.get_by_level_sum().find(level)->second
            , avg
        );
    }
    for (auto [steps, count] : results.get_by_steps_count()) {
        logger->info("Step count {} has {} matching items."
            , steps
            , count
        );
    }

    // Go home.
    return 0;
}
