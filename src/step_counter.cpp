#include <cstdint>
#include <filesystem>
#include <gmpxx.h>
#include <stdexcept>
#include <map>
#include "CLI.hpp"
#include "collatz/binary_tree_math.hpp"
#include "collatz/concepts.hpp"
#include "collatz/logging.hpp"
#include "collatz/collatz.hpp"
#include "collatz/progress.hpp"


typedef uint128_t tally_t;


class StepResults {
    private:

    std::map<size_t, tally_t> _by_level_sum;
    std::map<size_t, tally_t> _by_level_count;
    std::map<size_t, tally_t> _by_steps_count;

    public:
    StepResults() {}

    const std::map<size_t, tally_t>& get_by_level_sum() const { return _by_level_sum; }
    const std::map<size_t, tally_t>& get_by_level_count() const { return _by_level_count; }
    const std::map<size_t, tally_t>& get_by_steps_count() const { return _by_steps_count; }

    inline void add(size_t level, tally_t step_count) {
        _by_level_sum[level] += step_count;
        _by_level_count[level] += 1;
        _by_steps_count[step_count] += 1;
    }

    void merge(StepResults& other) {
        for (auto [level, sum] : other.get_by_level_sum()) {
            if (_by_level_sum.find(level) == _by_level_sum.end()) {
                _by_level_sum[level] = 0;
            }
            _by_level_sum[level] += sum;
        }
        for (auto [level, count] : other.get_by_level_count()) {
            if (_by_level_count.find(level) == _by_level_count.end()) {
                _by_level_count[level] = 0;
            }
            _by_level_count[level] += count;
        }
        for (auto [steps, count] : other.get_by_steps_count()) {
            if (_by_steps_count.find(steps) == _by_steps_count.end()) {
                _by_steps_count[steps] = 0;
            }
            _by_steps_count[steps] += count;
        }
    }
};



template<AnySupportedIntegral T>
StepResults run_it(size_t start_bit, size_t max_bit, uint128_t starting_value = 0) {
    StepResults results;
    size_t bit_limit = CollatzConstants::get_max_initial_value_max_bits<T>();
    T start_value = 0;
    T max_value = 0;
    size_t loop_limit;
    size_t loops_needed;
    size_t level;

    // Establish progress tracking.
    Progress progress(std::filesystem::current_path().string() + "/step_counter.progress");
    progress.monitor(start_value);

    // Go bit by bit.
    for (size_t bit = start_bit; bit < max_bit; bit++) {
        logger->debug("Processing bit {}.", bit);
        // Overflow check.
        if (bit > bit_limit) {
            if constexpr(NativeIntegral<T>) {
                StepResults other_results = run_it<uint128_t>(bit, max_bit);
                results.merge(other_results);
                return results;
            }
            throw std::out_of_range("This doesn't support over " + to_string_any(bit_limit) + " bits yet for overflow reasons.");
        }

        // Setup
        start_value = T(1) << bit;
        max_value = (T(1) << (bit + 1)) - 1;
        if (starting_value > max_value) {
            logger->warn(
                "Skipping bit {} because your starting_value {} is higher than this bit's max of {}."
                , bit
                , starting_value
                , max_value
            );
            continue;
        }
        if (starting_value > start_value) {
            start_value = starting_value;
        }
        loop_limit = SIZE_MAX;
        level = BinaryTreeMath<T>::st_node_level(start_value);
        if (level != BinaryTreeMath<T>::st_node_level(max_value)) {
            throw std::logic_error("Level for start_value and max_value didn't line up.  This is wrong.");
        }

        // Do the work in batches.  Gives us the ability to parallelize better.
        while(start_value <= max_value) {
            loops_needed = max_value - start_value + 1;  // +1 because inclusive counting
            if (loops_needed < loop_limit) {
                loop_limit = loops_needed;
            }

            // Do the loop
            #pragma omp parallel default(none) shared(start_value, loop_limit, level, results)
            {
                StepResults local_results;

                #pragma omp for schedule(static)
                for (size_t i = 0; i < loop_limit; i++) {
                    tally_t steps = Collatz<T>::st_get_step_count(start_value + i);
                    local_results.add(level, steps);
                }

                // Summarize.
                #pragma omp critical
                results.merge(local_results);
            }

            // Update the start_value.
            start_value += loop_limit;
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
    std::string starting_value_s;
    uint128_t starting_value;
    bool verbose;
    CLI::App options("Counts steps and emits them.");
    options.add_option("-b,--bits", max_bit, "Number of bits to test to.")->default_val(8);
    options.add_option("-s,--start", start_bit, "Bit numer to start at.")->default_val(0);
    options.add_option("-S,--starting-value", starting_value_s, "Use this starting value, if larger than bit mapped value (for continuation).")->default_val("0");
    options.add_flag(
        "-v,--verbose"
        , [&](size_t x){if(x>0) {verbose=true; logger->set_level(spdlog::level::debug);}}
        , "Enable verbosity."
    );
    CLI11_PARSE(options, argc, argv);
    starting_value = str_to_uint128(starting_value_s);
    logger->debug("Selected options were:");
    logger->debug("  Start Bit: {}", start_bit);
    logger->debug("  Starting Value: {}", starting_value);
    logger->debug("  Max Bit: {}", max_bit);
    logger->debug("  Verbose: {}", verbose);
    if (start_bit > max_bit) {
        logger->error("You can't set the start bit after the max bit.");
        exit(1);
    }

    // Run it.
    StepResults results;
    if (starting_value < UINT64_MAX) {
        results = run_it<uint64_t>(start_bit, max_bit, starting_value);
    } else {
        results = run_it<uint128_t>(start_bit, max_bit, starting_value);
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
