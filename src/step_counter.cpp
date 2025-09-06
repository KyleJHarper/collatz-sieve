#include <cstdint>
#include <gmpxx.h>
#include <stdexcept>
#include <map>
#include "CLI.hpp"
#include "collatz/binary_tree_math.hpp"
#include "collatz/concepts.hpp"
#include "collatz/logging.hpp"
#include "collatz/collatz.hpp"


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
        throw std::runtime_error("Not implemented");
    }
};



template<AnySupportedIntegral T>
StepResults run_it(size_t start_bit, size_t max_bit) {
    StepResults results;
    size_t bit_limit = CollatzConstants::get_max_initial_value_max_bits<T>();
    T start_value;
    T max_value;
    constexpr size_t BUFFER_SIZE = 100000;
    std::array<T, BUFFER_SIZE> buffer;
    size_t loop_limit;
    size_t loops_needed;
    size_t level;

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
        loop_limit = BUFFER_SIZE;
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
            #pragma omp parallel for default(none) schedule(static) shared(start_value, loop_limit, buffer)
            for (size_t i = 0; i < loop_limit; i++) {
                buffer[i] = Collatz<T>::st_get_step_count(start_value + i);
            }

            // Update results sequentially.
            for (size_t i = 0; i < loop_limit; i++) {
                results.add(level, buffer[i]);
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
    StepResults results = run_it<uint64_t>(start_bit, max_bit);

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
