#pragma once

#include "concepts.hpp"
#include "verifier_state.hpp"
#include "verifier.hpp"
#include "verifier_executor_policy.hpp"
#include "verifier_tls_metric.hpp"
#include "verifier_gpu_result_data.hpp"
#include "verifier_gpu_interface.hpp"
#include "cuda_stream_buffer.hpp"
#include <atomic>
#include <chrono>
#include <omp.h>
#include <stdexcept>
#include "gpu_support.hpp"



/**
* @class GPUVerifier
* @brief An implementation of the `Verifier` class which uses local GPUs to verfy survivors.
*/
template<FixedWidthIntegral T>
class GPUVerifier : public Verifier<T> {
    private:
    /**
    * @struct ThreadStorage
    * @brief Thread-Local storage, mostly for wake-up call tracking and metrics.
    *
    * For GPUs, this also includes `CHUNK` parameters.  The `NodeBitmap` behind the `tree` is leveraged to feed values to the GPU
    * via buffers (`CudaStreamBuffer`).  Since OMP is asynchronous, each thread is given chunk trackers to ensure they can operate
    * and fill the buffers independently without overlapping memory.  This avoids barriers and guards.
    *
    * @warning Data tends to spill cache lines and cause false sharing.  Do NOT remove alignas(64).
    */
    struct alignas(ABI::CACHE_LINE_SIZE) ThreadStorage {
        /// @brief How many more numbers can be checked (verified) before checking for a new state.
        size_t synchronization_countdown = 1;
        /// @brief Number of state checks performed.
        size_t synchronizations_performed = 0;
        /// @brief Metrics gathered during a thread's execution.
        VerifierTLSMetric metrics;

        /// @brief A chunk tracker for iteration and filling of the CudaStreamBuffer later.
        CudaStreamBufferChunk chunk;
    };

    /// @brief Thread-local storage for the main workers.  Should NOT be modified by anything other than the worker thread and sub-threads.
    std::vector<ThreadStorage> _tls;



    public:
    /// @name Lifecycle Management
    /// @{

    /// @brief Default constructor, not allowed.  Must give a tree reference.
    GPUVerifier() = delete;



    /// @brief Constructor taking a tree.
    explicit GPUVerifier(Verifier<T>::TreeType& tree) : Verifier<T>(tree) {
        if (!can_use_gpu()) {
            throw std::logic_error("No GPU detected.  Cannot build a GPUVerifier.");
        }
    }



    /// @brief Disallow copying.
    GPUVerifier(const GPUVerifier&) = delete;
    /// @brief Disallow copying.
    GPUVerifier& operator=(const GPUVerifier&) = delete;



    /// @brief Moving is okay.
    GPUVerifier(GPUVerifier&&) noexcept = default;
    /// @brief Moving is okay.
    GPUVerifier& operator=(GPUVerifier&&) noexcept = default;



    /// @brief Destruction is default for now.
    ~GPUVerifier() {
    }

    /// @}




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
            run_executor_impl<true>();
        } else {
            run_executor_impl<false>();
        }
    }



    /**
    * @brief The actual implementation of verification steps, with necessary template params/constexpr values sent.
    * @tparam UseIVTable Whether or not to use the max initial value table from `CollatzConstants` to protect from overflow instead
    * of tracking headroom bits in realtime.  Note: when the table is exhausted, the system falls back to headroom bits, making
    * this always safe to send as `true`.
    * @tparam DetailedMetrics Uses `verify_with_detailed_metrics()` in this implementation instead of `Collatz<T>::st_verify()`.
    */
    template<bool UseIVTable>
    void run_executor_impl() {
        // Build buffers.
        CudaStreamBuffer<T, GPUVerifierResultData> buffer(1ULL << 28);
        T* host_data_buffer = buffer.get_host_data_ptr();
        GPUVerifierResultData* host_results = buffer.get_host_results_ptr();

        // Process all the uncovered values.
        // Use SERIAL (or OMP num threads = 1) for now because parallel creates AB-BA race conditions.
        this->_tree.for_each_uncovered_value_with_tls(ForEachPolicy::PARALLEL, _tls, [&](T& value, ThreadStorage& thread_storage) {
            // Stop if done.  Edge-value handling is clearly documented to callers.
            if (this->_end_value > 0 && value > this->_end_value) {
                return ForEachSignal::BREAK;
            }

            // When this chunk is finished, return it and try to get another one.
            if (thread_storage.chunk.finished()) {
                // Return the chunk.  Let's buffer know we're done with it.
                buffer.return_chunk(thread_storage.chunk);

                // If the buffer is full, process it.
                while (buffer.get_next_chunk(thread_storage.chunk) == false) {
                    // Buffer is full.  The return_chunk() set it to FILLED. Send it.
                    // Mark buffer READY.
                    buffer.set_state(CudaStreamBufferState::READY);

                    // Send the buffer to the kernel for processing
                    buffer.launch_kernel(launch_gpu_verify_kernel<T, UseIVTable>);

                    // Wait for the buffer to be processed.
                    buffer.wait_until_state(CudaStreamBufferState::PROCESSED);

                    // Process result data.
                    if (host_results->overflow_count > 0) {
                        // Had at least one overflow.  Check if we contained them.
                        if (host_results->overflow_exceeded) {
                            // Ran out of overflow slots.  Must reprocess the whole batch on CPU.
                            for (uint64_t i = 0; i < buffer.get_element_count(); i++) {
                                T value = *(host_data_buffer + i);
                                if (! Collatz<T>::st_verify(value, value)) {
                                    throw std::logic_error("Failed to verify value: " + to_string_any(value));
                                }
                            }
                        } else {
                            // Contained the overflow.  Process them by themselves on CPU.
                            for (uint64_t i = 0; i < host_results->overflow_count; i++) {
                                T value = *(host_data_buffer + host_results->overflow_indexes[i]);
                                if (!Collatz<T>::st_verify(value, value)) {
                                    throw std::logic_error("Failed to verify value: " + to_string_any(value));
                                }
                            }
                        }
                    }

                    // Reset the results.
                    host_results->reset();

                    // Update metrics.
                    this->_published_metrics.nodes_verified_atomic.fetch_add(buffer.get_element_count());

                    // Recycle the buffer.  This resets counters and sets state to EMPTY for worker threads.
                    buffer.recycle();
                }
            }

            // With a valid chunk, add the value to the correct region in the buffer.
            host_data_buffer[thread_storage.chunk.buffer_index++] = value;

            // Synchronize state and metrics if necessary.
            if (--thread_storage.synchronization_countdown == 0) {
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

            // Continue
            return ForEachSignal::CONTINUE;
        }, this->_start_value);

        // Set state to STOPPED.  Only this thread may do that.
        this->_state.store(VerifierState::STOPPED);

        // Synchronize the timer since it's done doing any work.
        this->sync_timer();
    }

};
