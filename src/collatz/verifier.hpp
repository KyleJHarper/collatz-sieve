#pragma once
#include "verifier_state.hpp"
#include "verifier_metric.hpp"
#include "collatz_affine_stride.hpp"
#include "collatz_constants.hpp"
#include "concepts.hpp"
#include "binary_tree.hpp"
#include "int128.hpp"
#include <atomic>
#include <chrono>
#include <gmp.h>
#include <immintrin.h>
#include <stdexcept>
#include <thread>




/**
* @class Verifier
* @brief Verifies the uncovered values leftover in a `BinaryTree`.  Supports pausing, resuming, and detailed metrics.
*
* @tparam T Any supported integral (see concepts.hpp).
*/
template<AnySupportedIntegral T>
class Verifier {
    private:
    /**
    * @struct ThreadStorage
    * @brief Thread-Local storage for the verifier, mostly for wake-up call tracking and metrics, if desired.
    */
    struct ThreadStorage {
        /// @brief How many more numbers can be checked (verified) before checking for a new state.
        size_t state_check_countdown = 0;
        /// @brief Number of state checks performed.
        size_t state_check_countdowns_reached = 100000;
        /// @brief Metrics gathered during a thread's execution.
        VerifierMetric<T> metrics;
    };

    /// @brief The binary tree reference from the caller.
    const BinaryTree<T, BinaryTreeImplicitImpl<T>>& _tree;
    /// @brief Starting value when beginning iteration.  If zero (or lower than the tree's size), it bumps forward.
    T _start_value = 0;
    /// @brief Optional stopping value.  When zero, never stops.
    T _end_value = 0;
    /// @brief Worker thread where the main verification work happens.
    std::thread _worker;
    /// @brief How many verifications a thread should do before checking for a state change.
    size_t _state_check_countdown = 100000;
    /// @brief When the system is paused, how long should threads sleep before checking for a state change.
    size_t _paused_check_ms = 100;
    /// @brief Thread-local storage for the main workers.  Should NOT be modified by anything other than the worker thread and sub-threads.
    std::vector<ThreadStorage> _tls;
    /// @brief The state of the system.  See `VerifierState` for options.
    std::atomic<VerifierState> _state = VerifierState::STOPPED;
    /// @brief The last time `start()` or `resume` were called.
    std::chrono::time_point<std::chrono::steady_clock> _timer_start = std::chrono::steady_clock::now();
    /// @brief The cumulative duration in milliseconds this verifier has ran.
    std::chrono::milliseconds _duration_ms = std::chrono::milliseconds(0);



    /// @brief Starts the timer by setting it to now.
    void start_timer() {
        _timer_start = std::chrono::steady_clock::now();
    }



    /// @brief Stop the timer and increment the `_duration_ms`.
    void stop_timer() {
        _duration_ms += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _timer_start);
    }



    public:
    /// @name Lifecycle Management
    /// @{

    /// @brief Default constructor, not allowed.  Must give a tree reference.
    Verifier() = delete;



    /// @brief Constructor taking a tree.
    explicit Verifier(BinaryTree<T, BinaryTreeImplicitImpl<T>>& tree) : _tree(tree) {}



    /// @brief Disallow copying.
    Verifier(const Verifier&) = delete;
    /// @brief Disallow copying.
    Verifier& operator=(const Verifier&) = delete;



    /// @brief Moving is okay.
    Verifier(Verifier&&) noexcept = default;
    /// @brief Moving is okay.
    Verifier& operator=(Verifier&&) noexcept = default;



    /// @brief Destruction is default for now.
    ~Verifier() {
        if (_state.load(std::memory_order_acquire) != VerifierState::STOPPED) {
            stop();
        }
        if (_worker.joinable()) {
            _worker.join();
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

    /// @}



    /**
    * @brief Get the total amount of time the verifier has been in the running state.
    * @return Total milliseconds of duration.
    */
    uint64_t get_duration_ms() const {
        uint64_t total_ms = _duration_ms.count();
        if (_state.load(std::memory_order_relaxed) == VerifierState::RUNNING) {
            total_ms += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _timer_start).count();
        }
        return total_ms;
    }



    /**
    * @brief Gathers all the metrics from the workers.
    * @return A `VerifierMetric` object with all the thread-local values summarized.
    */
    const VerifierMetric<T> gather_metrics() const {
        VerifierMetric<T> metrics;
        for (const ThreadStorage& thread_storage : _tls) {
            metrics.merge(thread_storage.metrics);
        }
        metrics.duration_ms = _duration_ms;
        if (_state.load(std::memory_order_relaxed) == VerifierState::RUNNING) {
            metrics.duration_ms += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _timer_start);
        }
        return metrics;
    }



    /**
    * @brief Verifies a single value, promoting type if necessary.
    * @param value The value to verify.
    * @param thread_storage The caller's thread storage object for tracking and metric data.
    * @tparam U Any supported integral (see concepts.hpp).  Allows this method to promote from the `T` provided by the class.
    * @return True if verified, hangs otherwise.
    */
    template<AnySupportedIntegral U>
    inline bool verify_with_metrics_safe(U& value, ThreadStorage& thread_storage) {
        if constexpr(FixedWidthIntegral<U>) {
            if constexpr(sizeof(U) * 8 < 64) {
                // Type is less than 64 bits, just send it as a 64-bit.  Even uint32_t can't reach an overflow of 64-bit.
                // Max initial value of 64 bit is 12,327,829,502 which is greater than 2^32 which is 4,294,967,296.
                uint64_t safe_value = value;
                return verify_with_metrics_safe(safe_value, thread_storage);
            } else if constexpr(sizeof(U) * 8 == 64) {
                // Dealing with T of exactly 64 bits.  If it'll overflow, use 128-bit.  Otherwise, pass through.
                if (value > CollatzConstants::get_max_initial_value_by_type<U>()) {
                    uint128_t safe_value = value;
                    return verify_with_metrics_safe(safe_value, thread_storage);
                }
            } else if constexpr(sizeof(U) * 8 == 128) {
                // Dealing with 128 bits.  If it'll overflow, use mpz_class.
                if (value > CollatzConstants::get_max_initial_value_by_type<U>()) {
                    static thread_local mpz_class safe_value;
                    Int128::uint128_to_mpz(value, safe_value);
                    return verify_with_metrics_safe(safe_value, thread_storage);
                }
            }
        }

        // May now process value, guaranteed to not overflow.
        bool below_hwm = false;
        size_t steps = 0;
        if constexpr(FixedWidthIntegral<U>) {
            U initial_value = value;
            while (value != 1) {
                if ((value & 1) == 0) {
                    value >>= 1;
                } else {
                    value = (value << 1) + value + 1;
                }
                // Tally the local step count.
                steps++;

                // High-Water Mark Metrics
                if (below_hwm) {
                    thread_storage.metrics.steps_skippable_by_hwm++;
                } else {
                    if (value < initial_value) {
                        below_hwm = true;
                        // Affine stride is no longer effective.  Tally it by counting how many strides it would've taken.
                        thread_storage.metrics.steps_skippable_by_affine_stride_before_hwm += (steps - (1 + (steps / AffineStride::STRIDE_SIZE)));
                    }
                }
            }
        } else if constexpr(GMPIntegral<U>) {
            static thread_local U initial_value;
            initial_value = value;
            while (mpz_cmp_ui(value.get_mpz_t(), 1) != 0) {
                if (mpz_even_p(value.get_mpz_t())) {
                    mpz_tdiv_q_2exp(value.get_mpz_t(), value.get_mpz_t(), 1);
                } else {
                    mpz_mul_ui(value.get_mpz_t(), value.get_mpz_t(), 3);
                    mpz_add_ui(value.get_mpz_t(), value.get_mpz_t(), 1);
                }

                // Tally the local step count.
                steps++;

                // High-Water Mark Metrics
                if (below_hwm) {
                    thread_storage.metrics.steps_skippable_by_hwm++;
                } else {
                    if (value < initial_value) {
                        below_hwm = true;
                        // Affine stride is no longer effective.  Tally it by counting how many strides it would've taken.
                        thread_storage.metrics.steps_skippable_by_affine_stride_before_hwm += (steps - (1 + (steps / AffineStride::STRIDE_SIZE)));
                    }
                }
            }
        }
        // Add the local steps to the thread local total.
        thread_storage.metrics.steps_total += steps;

        // Affine Stride Metric
        // This is simply the number of whole strides that could've been taken times the number of steps saved.
        thread_storage.metrics.steps_skippable_by_affine_stride += ((steps / AffineStride::STRIDE_SIZE) * (AffineStride::STRIDE_SIZE - 1));

        // All done.  Return true.
        return true;
    }



    /// @name Job Control
    /// @{

    /**
    * @brief Starts the verifier.  Clears any previous state/data.  Puts the work into a sub-thread and then returns.
    * @tparam DetailedMetrics Boolean to determine if detailed metrics should be gathered.  When false, it runs faster obviously.
    */
    template<bool DetailedMetrics>
    void start() {
        // Need to check if the worker has already been started.
        if (_state.load(std::memory_order_relaxed) == VerifierState::RUNNING) {
            return;
        }

        // Cannot start running when the state is anything other than STOPPED.
        if (_state.load(std::memory_order_relaxed) != VerifierState::STOPPED) {
            throw std::logic_error("Cannot call start() unless the job is in the STOPPED state.");
        }

        // Join the previous thread, if possible, to make this more re-entrant safe.
        if (_worker.joinable()) {
            _worker.join();
        }

        // Reset variables.
        _tls.clear();
        _duration_ms = std::chrono::milliseconds(0);

        // Change the state.
        _state.store(VerifierState::RUNNING);

        // Start the timer.
        start_timer();

        // Start the worker thread and assign it.
        _worker = std::thread([this] {
            _tree.for_each_uncovered_value_with_tls(ForEachPolicy::PARALLEL, _tls, [&](T& value, ThreadStorage& thread_storage) {
                // Stop if done.
                if (_end_value > 0 && value > _end_value) {
                    return ForEachSignal::BREAK;
                }

                // Check for pausing and stopped if needed.
                if (thread_storage.state_check_countdown-- == 0) {
                    // Reset the countdown for next time.  Bump the counter for metrics.
                    thread_storage.state_check_countdown = _state_check_countdown;
                    thread_storage.state_check_countdowns_reached++;

                    // Wait when paused.
                    while (_state.load(std::memory_order_relaxed) == VerifierState::PAUSED) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(_paused_check_ms));
                    }

                    // Quit if necessary.
                    if (_state.load(std::memory_order_relaxed) == VerifierState::STOPPING) {
                        return ForEachSignal::BREAK;
                    }
                }

                // Verify using whichever method is requested: with or without metrics.
                if constexpr(DetailedMetrics) {
                    if (! verify_with_metrics_safe(value, thread_storage)) {
                        throw std::logic_error("Failed to validate value: " + to_string_any(value));
                    }
                } else {
                    // Handles overflow detection and promotes automatically.
                    // if (! Collatz<T>::st_verify(value, value)) {
                    if (! Collatz<T>::st_verify_to_hwm(value)) {
                        throw std::logic_error("Failed to validate value: " + to_string_any(value));
                    }
                }

                // Always bump the number of nodes verified, even without DetailedMetrics enabled.
                thread_storage.metrics.nodes_verified++;

                // Continue
                return ForEachSignal::CONTINUE;
            }, _start_value);

            // Set state to STOPPED.  Only this worker thread may do that.
            _state.store(VerifierState::STOPPED);

            // Stop the timer.
            stop_timer();
        });
    }



    /// @brief Stops verification, killing off threads.  Verification cannot be resumed via `resume()`.
    void stop() {
        // When stopped or stopping, the state is already correct.
        if (_state.load(std::memory_order_relaxed) == VerifierState::STOPPED || _state.load(std::memory_order_relaxed) == VerifierState::STOPPING) {
            return;
        }

        // Flag STOPPING (not stopped; only the _worker can flag STOPPED).
        _state.store(VerifierState::STOPPING);

        // Do not stop the timer here.  Worker thread will do that when all the child threads are done.
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

        // Stop timer.
        stop_timer();
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
