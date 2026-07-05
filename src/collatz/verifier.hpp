#pragma once
#include "verifier_state.hpp"
#include "verifier_metric.hpp"
#include "verifier_executor_policy.hpp"
#include "concepts.hpp"
#include "binary_tree.hpp"
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>




/**
* @class Verifier
* @brief A base class behind the verification implementation classes.  Provides the start, stop, resume, and so forth to manage the
* state and monitor the progress of a verification process.  Actual implementation details are defined in the subclasses.
*
* @tparam T Any supported integral (see concepts.hpp).
*/
template<AnySupportedIntegral T>
class Verifier {
    private:
    /// @brief Thread with the verification logic running.
    std::thread _executor_thread;



    protected:
    using TreeType = BinaryTree<T, BinaryTreeImplicitImpl<T>>;



    /// @brief The binary tree reference from the caller.
    const TreeType& _tree;



    /// @brief Starting value when beginning iteration.  If zero (or lower than the tree's size), it bumps forward.
    T _start_value = 0;



    /**
    * @brief Optional stopping value.  When zero, never stops.
    * @note Iteration, especially in the CPU version, use the NodeBitmap.  Read the warning about its parallel boundaries in the
    * README.md file.
    */
    T _end_value = 0;



    /// @brief The state of the system.  See `VerifierState` for options.
    std::atomic<VerifierState> _state = VerifierState::STOPPED;



    /// @brief How many verifications a thread should do before synchronizing: checking for state changes and updating metrics.
    /// @note Benchmarking showed almost no difference between 1,000 - 100,000,000 on the `CPUVerifier`.  It's set lower to favor
    /// responsiveness.
    size_t _synchronization_countdown = 100'000;



    /// @brief Controls how often paused loops check for updates.  This is a guaranteed minimum, not maximum.
    size_t _paused_check_ms = 100;



    /// @brief The last time `start()` or `resume()` was called.
    std::chrono::time_point<std::chrono::steady_clock> _timer_start = std::chrono::steady_clock::now();



    /// @brief A metric object, to be updated by executors when `_synchronization_countdown` is reached.
    VerifierMetric _published_metrics;



    /// @brief Starts the timer by setting it to now.
    void start_timer() {
        _timer_start = std::chrono::steady_clock::now();
    }



    /// @brief Synchronizes the current duration stored in _published_metrics.
    void sync_timer() {
        std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
        _published_metrics.duration_ms += std::chrono::duration_cast<std::chrono::milliseconds>(now - _timer_start);
        _timer_start = now;
    }



    /// @brief The actual execution function for the subclass to implement (CPU, GPU, etc).
    /// @param policy The execution policy for the executor to follow.  See verifier_executor_policy.hpp.
    virtual void run_executor(const VerifierExecutorPolicy& policy) = 0;



    public:
    /// @name Lifecycle Management
    /// @{

    /// @brief Default constructor, not allowed.  Must send a tree.
    Verifier() = delete;



    /// @brief Constructor taking a tree (and actually assigning it).  This class owns the tree.
    explicit Verifier(TreeType& tree) : _tree(tree) {}



    /// @brief Disallow copying.
    Verifier(const Verifier&) = delete;
    /// @brief Disallow copying.
    Verifier& operator=(const Verifier&) = delete;



    /// @brief Moving is okay.
    Verifier(Verifier&&) noexcept = default;
    /// @brief Moving is okay.
    Verifier& operator=(Verifier&&) noexcept = default;



    /// @brief When destroyed, call stop to flag STOPPING, and then join the executor thread.
    virtual ~Verifier() {
        if (_state.load(std::memory_order_acquire) != VerifierState::STOPPED) {
            stop();
        }
        if (_executor_thread.joinable()) {
            _executor_thread.join();
        }
    }

    /// @}



    /// @name Accessors
    /// @{

    /// @brief Get the start value.
    const T& get_start_value() const { return _start_value; }



    /// @brief Set the start value.  When higher than the tree's largest value, will skip forward within a range.  See `BinaryTree.hpp`.
    void set_start_value(const T& x) { _start_value = x; }



    /// @brief Get the end value.  When zero, it's considered infinite (no end).
    const T& get_end_value() const { return _end_value; }



    /// @brief Set the end value.  Useful for testing in batches or ranges.
    void set_end_value(const T& x) { _end_value = x; }



    /// @brief Get the current state.
    VerifierState get_state(std::memory_order m = std::memory_order_relaxed) const { return _state.load(m); }



    /// @brief Get the latest metrics published by the threads.
    const VerifierMetric& get_metrics() {
        sync_timer();
        return _published_metrics;
    }

    /// @}



    /// @name Job Control
    /// @{

    /**
    * @brief Starts the verifier.  Clears any previous state/data.  Puts the work into a sub-thread and then returns.
    * @param policy A set of flags for the executor to respect when processing values.
    */
    void start(VerifierExecutorPolicy policy) {
        // Need to check if the worker has already been started.
        if (_state.load(std::memory_order_relaxed) == VerifierState::RUNNING) {
            return;
        }

        // Cannot start running when the state is anything other than STOPPED.
        if (_state.load(std::memory_order_relaxed) != VerifierState::STOPPED) {
            throw std::logic_error("Cannot call start() unless the job is in the STOPPED state.");
        }

        // Join the previous executor thread (if any) to make this re-entrant safe-ish.
        if (_executor_thread.joinable()) {
            _executor_thread.join();
        }

        // Reset metrics and time tracking.
        _published_metrics.reset();

        // Change the state.
        _state.store(VerifierState::RUNNING);

        // Fire up the executor in a thread.
        _executor_thread = std::thread([this, policy] { run_executor(policy); });

        // Start the timer.
        start_timer();
    }



    /// @brief Stops verification, killing off threads.  Verification cannot be resumed via `resume()`.
    void stop() {
        // When stopped or stopping, the state is already correct.
        if (_state.load(std::memory_order_relaxed) == VerifierState::STOPPED || _state.load(std::memory_order_relaxed) == VerifierState::STOPPING) {
            return;
        }

        // Flag STOPPING (not stopped; only the _worker can flag STOPPED).
        _state.store(VerifierState::STOPPING);

        // Do not sync or stop the timer here.  Executor thread will do that when all the child threads are done.

        // Join the executor thread.  Both `start()` and the destructor have added protection for this, fyi.
        if (_executor_thread.joinable()) {
            _executor_thread.join();
        }
    }



    /// @brief Pauses verification, putting threads to sleep.  Verification may continue via `resume()`.
    void pause() {
        // When already paused, do nothing.
        if (_state.load(std::memory_order_relaxed) == VerifierState::PAUSED) {
            return;
        }

        // If stopped, exception.  Cannot pause a stopped worker.
        if (_state.load(std::memory_order_relaxed) == VerifierState::STOPPED || _state.load(std::memory_order_relaxed) == VerifierState::STOPPING) {
            throw std::logic_error("Cannot call pause() when a worker is STOPPED or STOPPING.");
        }

        // Flag PAUSED.  Worker will read this and go to sleep eventually.
        _state.store(VerifierState::PAUSED);

        // Sync the timer.  The `resume()` method will call `start_timer()` to reset it.
        sync_timer();
    }



    /// @brief Wakes threads from sleep following a `pause()`.
    void resume() {
        // When already running, do nothing.
        if (_state.load(std::memory_order_relaxed) == VerifierState::RUNNING) {
            return;
        }

        // If stopped, exception.  Cannot resume a stopped worker.
        if (_state.load(std::memory_order_relaxed) == VerifierState::STOPPED || _state.load(std::memory_order_relaxed) == VerifierState::STOPPING) {
            throw std::logic_error("Cannot call resume() when a worker is STOPPED or STOPPING.");
        }

        // Flag RUNNING.  Worker will read this and resume operation.
        _state.store(VerifierState::RUNNING);

        // Start the timer again.
        start_timer();
    }

    /// @}

};
