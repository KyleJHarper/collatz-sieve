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
#include <memory>
#include <omp.h>
#include <stdexcept>
#include "gpu.hpp"



/**
* @class GPUVerifier__Old
* @brief An implementation of the `Verifier` class which uses local GPUs to verfy survivors.
* @warning This class uses the tree's iterator, which is convenient because it handles the scaling factor and such for you, but the
* updated `GPUVerifier` is faster and uses less CPU power ... and you should use it instead.
*/
template<FixedWidthIntegral T>
class GPUVerifier__Old : public Verifier<T> {
    private:
    /**
    * @struct ThreadStorage
    * @brief Thread-Local storage, mostly for wake-up call tracking and metrics.
    *
    * For GPUs, this also includes a `CudaStreamBuffer` object.  Since OMP has implicit barriers which were intentionally leveraged
    * at the prefix level in the parallel iterator, AB-BA deadlocks can occur if ANY blocking synchronization technique attempts to
    * coordinate threads within a for_each iterator.  For this reason, each thread gets its own buffer to fill, submit, and wait on
    * without barriers from others.
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

        /// @brief A pointer for a full CudaStreamBuffer object.  Avoids ABA / AB-BA problems.
        std::unique_ptr<CudaStreamBuffer<T, GPUVerifierResultData>> buffer_ptr;
    };

    /// @brief Thread-local storage for the main workers.  Should NOT be modified by anything other than the worker thread and sub-threads.
    std::vector<ThreadStorage> _tls;



    public:
    /// @name Lifecycle Management
    /// @{

    /// @brief Default constructor, not allowed.  Must give a tree reference.
    GPUVerifier__Old() = delete;



    /// @brief Constructor taking a tree.
    explicit GPUVerifier__Old(Verifier<T>::TreeType& tree) : Verifier<T>(tree) {
        if (!GPU::can_use_gpu()) {
            throw std::logic_error("No GPU detected.  Cannot build a GPUVerifier.");
        }
        GPU::initialize_stride_table();
    }



    /// @brief Disallow copying.
    GPUVerifier__Old(const GPUVerifier__Old&) = delete;
    /// @brief Disallow copying.
    GPUVerifier__Old& operator=(const GPUVerifier__Old&) = delete;



    /// @brief Moving is okay.
    GPUVerifier__Old(GPUVerifier__Old&&) noexcept = default;
    /// @brief Moving is okay.
    GPUVerifier__Old& operator=(GPUVerifier__Old&&) noexcept = default;



    /// @brief Destruction is default for now.
    ~GPUVerifier__Old() {
    }

    /// @}




    /**
    * @brief Entry point for the `Verifier` base class to begin actual execution of number verification.
    *
    * In this GPU implementation, it's largely useless.
    *
    * @param policy A verification policy to follow.  (see verifier_executor_policy.hpp)
    */
    void run_executor(const VerifierExecutorPolicy& policy) override {
        // Dispatch runtime selection to Impl.
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
    */
    template<bool UseIVTable>
    void run_executor_impl(size_t gpu_buffer_limit = 0) {
        // Find out how much memory is available for the GPU.  Start by assuming the caller sent a fixed byte amount.
        size_t total_buffer_limit = gpu_buffer_limit;

        // If unsent, aim for 90% of the free bytes.
        if (gpu_buffer_limit == 0) {
            size_t free_bytes = 0;
            size_t total_bytes = 0;
            cudaError_t status = cudaMemGetInfo(&free_bytes, &total_bytes);
            if (status != cudaSuccess) {
                throw std::runtime_error(std::string("CUDA Error: ") + cudaGetErrorString(status));
            }
            total_buffer_limit = free_bytes * 9 / 10;  // 90%
        }

        // Use the total buffer limit to decide how large each thread's buffer can be.
        const size_t thread_count = static_cast<size_t>(omp_get_max_threads());
        const size_t buffer_limit_per_thread = total_buffer_limit / thread_count;

        // Use size_of to determine how many elements that works out to be.  It'll be used by the buffer's constructor.
        const uint64_t elements_per_thread = buffer_limit_per_thread / sizeof(T);

        // Build the TLS objects so element count can be sent.  Otherwise NodeBitmap will default-initialiaze and fail.
        _tls.resize(thread_count);
        for (ThreadStorage& tls : _tls) {
            tls.buffer_ptr = std::make_unique<CudaStreamBuffer<T, GPUVerifierResultData>>(elements_per_thread);
        }

        // Loop through all the values, slamming them into per-thread buffers and sending them to the GPU for processing.
        this->_tree.for_each_uncovered_value_with_tls(ForEachPolicy::PARALLEL, _tls, [&](T& value, ThreadStorage& thread_storage) {
            // Stop if done.  Edge-value handling is clearly documented to callers.
            if (this->_end_value > 0 && value > this->_end_value) {
                return ForEachSignal::BREAK;
            }

            // If the buffer is full, send it to the GPU for processing.
            if (thread_storage.buffer_ptr->is_full()) {
                // Call helper to process on the GPU.
                process_buffer<UseIVTable>(thread_storage);

                // This is a natual point to check for pausing, stopping, etc.
                while (this->_state.load(std::memory_order_relaxed) == VerifierState::PAUSED) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(this->_paused_check_ms));
                }
                if (this->_state.load(std::memory_order_relaxed) == VerifierState::STOPPING) {
                    return ForEachSignal::BREAK;
                }
                thread_storage.synchronizations_performed++;
            }

            // Buffer has space.  Add the current value to the next index, incrementing it too.
            thread_storage.buffer_ptr->get_host_data_ptr()[thread_storage.buffer_ptr->use_next_index()] = value;

            // Continue
            return ForEachSignal::CONTINUE;
        }, this->_start_value);

        // Finalize any pending buffers.
        for (ThreadStorage& thread_storage : _tls) {
            if (thread_storage.buffer_ptr->get_next_index() > 0) {
                process_buffer<UseIVTable>(thread_storage);
            }
        }

        // Set state to STOPPED.  Only this thread may do that.
        this->_state.store(VerifierState::STOPPED);

        // Synchronize the timer since it's done doing any work.
        this->sync_timer();
    }



    /// @brief Helper to process a full buffer.
    template<bool UseIVTable>
    void process_buffer(ThreadStorage& thread_storage) {
        // Mark the buffer READY.
        thread_storage.buffer_ptr->set_state(CudaStreamBufferState::READY);

        // Launch the kernel for the GPU to execute when it's able to.
        thread_storage.buffer_ptr->launch_kernel(launch_gpu_verify_kernel<T, UseIVTable>);

        // Wait for the GPU to mark the buffer PROCESSED.
        thread_storage.buffer_ptr->wait_until_state(CudaStreamBufferState::PROCESSED);

        // Process the results.  Check for overflows and handle them.
        GPUVerifierResultData* host_results_ptr = thread_storage.buffer_ptr->get_host_results_ptr();
        T* host_data_ptr = thread_storage.buffer_ptr->get_host_data_ptr();
        if (host_results_ptr->overflow_count > 0) {
            // Had at least one overflow.  Check if we contained them.
            if (host_results_ptr->overflow_exceeded) {
                // Ran out of overflow slots.  Must reprocess the whole batch on CPU.
                for (uint64_t i = 0; i < thread_storage.buffer_ptr->get_next_index(); i++) {
                    T value = *(host_data_ptr + i);
                    if (! Collatz<T>::st_verify(value, value)) {
                        throw std::logic_error("Failed to verify value: " + to_string_any(value));
                    }
                }
                // Update the metrics for this, including the overflow-exceeded counter.
                this->_published_metrics.gpu_overflow_buffer_exceeded.fetch_add(1);
                this->_published_metrics.gpu_overflows_processed.fetch_add(thread_storage.buffer_ptr->get_next_index() + 1);
            } else {
                // Contained the overflow.  Process only the offenders on CPU.
                for (uint64_t i = 0; i < host_results_ptr->overflow_count; i++) {
                    T value = *(host_data_ptr + host_results_ptr->overflow_indexes[i]);
                    if (!Collatz<T>::st_verify(value, value)) {
                        throw std::logic_error("Failed to verify value: " + to_string_any(value));
                    }
                }
                // Update the metrics for this.
                this->_published_metrics.gpu_overflows_processed.fetch_add(host_results_ptr->overflow_count);
            }
        }

        // Reset the results.  They'll be sync'd as-such to the device.
        host_results_ptr->reset();

        // Update metrics.
        this->_published_metrics.nodes_verified_atomic.fetch_add(thread_storage.buffer_ptr->get_next_index());
        this->_published_metrics.gpu_kernel_launches.fetch_add(1);

        // Recycle the buffer.  This resets counters and sets state to FILLING.
        thread_storage.buffer_ptr->recycle();
    }

};
