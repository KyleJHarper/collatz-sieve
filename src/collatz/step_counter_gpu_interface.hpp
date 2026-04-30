#include "concepts_for_cuda.hpp"
#include <stdint.h>
#include <map>



typedef uint128_t tally_t;



/**
* @class StepResults
* @brief Storage class for the step-counter program.
* @note Tallies are stored with `uint128_t`.
*/
class StepResults {
    private:
    /// @brief Number of steps of all nodes on a level.
    std::map<size_t, tally_t> _by_level_sum;
    /// @brief Number of items (values) reviewed on a given level.
    std::map<size_t, tally_t> _by_level_count;
    /// @brief Number of times a step count appeared.
    std::map<size_t, tally_t> _by_steps_count;



    public:
    /// @name Lifecycle Management
    /// @{

    /// @brief Default constructor.
    StepResults() {}

    /// @}



    /// @name Accesstors
    /// @{

    /// @brief Return the map tracking the number of steps of all nodes on each level.
    const std::map<size_t, tally_t>& get_by_level_sum() const { return _by_level_sum; }



    /// @brief Return the map tracking the number of items (values) found on each level.
    const std::map<size_t, tally_t>& get_by_level_count() const { return _by_level_count; }



    /// @brief Return the map tracking the count of times a step-count happened.
    const std::map<size_t, tally_t>& get_by_steps_count() const { return _by_steps_count; }

    /// @}



    /// @brief Add a step count to the given level.
    inline void add(size_t level, tally_t step_count) {
        _by_level_sum[level] += step_count;
        _by_level_count[level] += 1;
        _by_steps_count[step_count] += 1;
    }



    /// @brief Add an aggregate number of steps, count times.
    inline void add_aggregate(size_t level, tally_t step, tally_t count) {
        _by_level_sum[level] += (step * count);
        _by_level_count[level] += count;
        _by_steps_count[step] += count;
    }



    /// @brief Merges the results of another `StepResults` to this one.  Usually for threading reasons.
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




/// @brief Expose the CollatzStepRunner in the .cu file.
template<typename T>
class CollatzStepRunner;



/// @brief Expose the create_runner in the .cu file.
template<typename T>
CollatzStepRunner<T>* create_runner(size_t buffer_size, T* unified_start_value_ptr);



/// @brief Expose the destroy_runner in the .cu file.
template<typename T>
void destroy_runner(CollatzStepRunner<T>* runner);



/// @brief Expose the process_level_gpu in the .cu file.
template<typename T>
void process_level_gpu(CollatzStepRunner<T>* runner, T max_value, size_t level, StepResults *global_results);
