#include "concepts_for_cuda.hpp"
#include <stdint.h>
#include <map>



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

    inline void add_aggregate(size_t level, tally_t step, tally_t count) {
        _by_level_sum[level] += (step * count);
        _by_level_count[level] += count;
        _by_steps_count[step] += count;
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



// Interface for the GPU functions.
template<typename T>
class CollatzStepRunner;

template<typename T>
CollatzStepRunner<T>* create_runner(size_t buffer_size, T* unified_start_value_ptr);

template<typename T>
void destroy_runner(CollatzStepRunner<T>* runner);

template<typename T>
void process_level_gpu(CollatzStepRunner<T>* runner, T max_value, size_t level, StepResults *global_results);
