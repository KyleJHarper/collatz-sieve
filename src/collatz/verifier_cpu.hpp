#pragma once

#include "abi.hpp"
#include "concepts.hpp"
#include "verifier_tls_metric.hpp"
#include "verifier_state.hpp"
#include "verifier.hpp"
#include "verifier_executor_policy.hpp"
#include <atomic>



/**
* @class CPUVerifier
* @brief An implementation of the `Verifier` class which uses local CPUs to process values.
*/
template<AnySupportedIntegral T>
class CPUVerifier : public Verifier<T> {
    private:
    /**
    * @struct ThreadStorage
    * @brief Thread-Local storage, mostly for wake-up call tracking and metrics.
    * @warning Data tends to spill cache lines and cause false sharing.  Do NOT remove alignas(64).
    */
    struct alignas(ABI::CACHE_LINE_SIZE) ThreadStorage {
        /// @brief How many more numbers can be checked (verified) before checking for a new state.
        size_t synchronization_countdown = 1;
        /// @brief Number of state checks performed.
        size_t synchronizations_performed = 0;
        /// @brief Metrics gathered during a thread's execution.
        VerifierTLSMetric metrics;
    };

    /// @brief Thread-local storage for the main workers.  Should NOT be modified by anything other than the worker thread and sub-threads.
    std::vector<ThreadStorage> _tls;



    public:
    /// @name Lifecycle Management
    /// @{

    /// @brief Default constructor, not allowed.  Must give a tree reference.
    CPUVerifier() = delete;



    /// @brief Constructor taking a tree.
    explicit CPUVerifier(Verifier<T>::TreeType& tree) : Verifier<T>(tree) {}



    /// @brief Disallow copying.
    CPUVerifier(const CPUVerifier&) = delete;
    /// @brief Disallow copying.
    CPUVerifier& operator=(const CPUVerifier&) = delete;



    /// @brief Moving is okay.
    CPUVerifier(CPUVerifier&&) noexcept = default;
    /// @brief Moving is okay.
    CPUVerifier& operator=(CPUVerifier&&) noexcept = default;



    /// @brief Destruction is default for now.
    ~CPUVerifier() {
    }

    /// @}



    /**
    * @brief Verifies a single value, promoting type if necessary.
    * @param value The value to verify.
    * @param thread_storage The caller's thread storage object for tracking and metric data.
    * @tparam U Any supported integral (see concepts.hpp).  Allows this method to promote from the `T` provided by the class.
    * @return True if verified, hangs otherwise.
    */
    template<AnySupportedIntegral U>
    inline bool verify_with_detailed_metrics(U& value, ThreadStorage& thread_storage) {
        // Test for overflow.
        // This method does NOT use striding for obvious reasons.  Lean on the precomputed max initial values tables.
        if constexpr(FixedWidthIntegral<U>) {
            if constexpr(sizeof(U) * 8 < 64) {
                // Type is less than 64 bits, just send it as a 64-bit.  Even uint32_t can't reach an overflow of 64-bit.
                // Max initial value of 64 bit is 12,327,829,502 which is greater than 2^32 which is 4,294,967,296.
                uint64_t safe_value = value;
                return verify_with_detailed_metrics(safe_value, thread_storage);
            } else if constexpr(sizeof(U) * 8 == 64) {
                // Dealing with T of exactly 64 bits.  If it'll overflow, use 128-bit.  Otherwise, pass through.
                if (value > CollatzConstants::get_max_initial_value_by_type<U>()) {
                    uint128_t safe_value = value;
                    return verify_with_detailed_metrics(safe_value, thread_storage);
                }
            } else if constexpr(sizeof(U) * 8 == 128) {
                // Dealing with 128 bits.  If it'll overflow, use mpz_class.
                if (value > CollatzConstants::get_max_initial_value_by_type<U>()) {
                    static thread_local mpz_class safe_value;
                    Int128::uint128_to_mpz(value, safe_value);
                    return verify_with_detailed_metrics(safe_value, thread_storage);
                }
            }
        }

        // May now process value, guaranteed to not overflow.
        bool below_hwm = false;
        size_t steps = 0;
        using StrideTable = std::conditional_t<
            FixedWidthIntegral<T>
            , AffineStride::VerifyFWTable
            , AffineStride::VerifyGMPTable
        >;
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
                        thread_storage.metrics.steps_skippable_by_affine_stride_before_hwm += (steps - (1 + (steps / StrideTable::STRIDE_SIZE)));
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
                        thread_storage.metrics.steps_skippable_by_affine_stride_before_hwm += (steps - (1 + (steps / StrideTable::STRIDE_SIZE)));
                    }
                }
            }
        }
        // Add the local steps to the thread local total.
        thread_storage.metrics.steps_total += steps;

        // Affine Stride Metric
        // This is simply the number of whole strides that could've been taken times the number of steps saved.
        thread_storage.metrics.steps_skippable_by_affine_stride += ((steps / StrideTable::STRIDE_SIZE) * (StrideTable::STRIDE_SIZE - 1));

        // All done.  Return true.
        return true;
    }



    /**
    * @brief Entry point for the `Verifier` base class to begin actual execution of number verification.
    *
    * In this CPU implementation, this is actually just a dispatcher.  It takes the runtime policy choices and makes a single call
    * to the `run_executor_impl()` with the neccessary compile-time constants.  This avoids if/else blocks inside hot loops which
    * adds anywhere from 1-2% up to 10-20% performance in testing.
    *
    * @param policy A verification policy to follow.  (see verifier_executor_policy.hpp)
    */
    void run_executor(const VerifierExecutorPolicy& policy) override {
        // Dispatch runtime selection to Impl
        if (policy.enable_max_iv_table) {
            if (policy.detailed_metrics) {
                run_executor_impl<true, true>();
            } else {
                run_executor_impl<true, false>();
            }
        } else {
            if (policy.detailed_metrics) {
                run_executor_impl<false, true>();
            } else {
                run_executor_impl<false, false>();
            }
        }
    }



    /**
    * @brief The actual implementation of verification steps, with necessary template params/constexpr values sent.
    * @tparam UseIVTable Whether or not to use the max initial value table from `CollatzConstants` to protect from overflow instead
    * of tracking headroom bits in realtime.  Note: when the table is exhausted, the system falls back to headroom bits, making
    * this always safe to send as `true`.
    * @tparam DetailedMetrics Uses `verify_with_detailed_metrics()` in this implementation instead of `Collatz<T>::st_verify()`.
    */
    template<bool UseIVTable, bool DetailedMetrics>
    void run_executor_impl() {
        // Reset variables.
        _tls.clear();

        // Process all the uncovered values.
        this->_tree.for_each_uncovered_value_with_tls(ForEachPolicy::PARALLEL, _tls, [&](T& value, ThreadStorage& thread_storage) {
            // Stop if done.
            if (this->_end_value > 0 && value > this->_end_value) {
                thread_storage.metrics.flush(this->_published_metrics, true);
                return ForEachSignal::BREAK;
            }

            // Synchronize state and metrics if necessary.
            if (thread_storage.synchronization_countdown-- == 0) {
                // Reset the countdown for next time.  Bump the counter for metrics.
                thread_storage.synchronization_countdown = this->_synchronization_countdown;
                thread_storage.synchronizations_performed++;

                // Flush metrics.
                thread_storage.metrics.flush(this->_published_metrics, true);

                // Wait when paused.
                while (this->_state.load(std::memory_order_relaxed) == VerifierState::PAUSED) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(this->_paused_check_ms));
                }

                // Quit if necessary.
                if (this->_state.load(std::memory_order_relaxed) == VerifierState::STOPPING) {
                    return ForEachSignal::BREAK;
                }
            }

            // Verify using whichever method is requested: with or without metrics.
            if constexpr(DetailedMetrics) {
                if (! verify_with_detailed_metrics(value, thread_storage)) {
                    throw std::logic_error("Failed to validate value: " + to_string_any(value));
                }
            } else {
                // Handles overflow detection and promotes automatically.
                if (! Collatz<T>::template st_verify<T, UseIVTable>(value, value)) {
                    throw std::logic_error("Failed to validate value: " + to_string_any(value));
                }
            }

            // Always bump the number of nodes verified, even without DetailedMetrics enabled.
            thread_storage.metrics.nodes_verified++;

            // Continue
            return ForEachSignal::CONTINUE;
        }, this->_start_value);

        // Set state to STOPPED.  Only this thread may do that.
        this->_state.store(VerifierState::STOPPED);

        // Synchronize the timer since it's done doing any work.
        this->sync_timer();
    }

};
