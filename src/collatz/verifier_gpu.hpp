#pragma once

#include "concepts.hpp"
#include "for_each.hpp"
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
* @class GPUVerifier
* @brief An implementation of the `Verifier` class which uses local GPUs to verfy survivors.
* @tparam T Any fixed-width type (up to 128-bit).
*/
template<FixedWidthIntegral T>
class GPUVerifier : public Verifier<T> {
    private:
    const T _scaling_factor;
    size_t _base_multiplier = 0;
    size_t _max_multiplier = 0;



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
    GPUVerifier() = delete;



    /// @brief Constructor taking a tree.
    explicit GPUVerifier(Verifier<T>::TreeType& tree) : Verifier<T>(tree), _scaling_factor(tree.get_scaling_factor()) {
        if (!GPU::can_use_gpu()) {
            throw std::logic_error("No GPU detected.  Cannot build a GPUVerifier.");
        }
        GPU::initialize_stride_table();
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
    * @brief The actual implementation of verification steps, with necessary template params/constexpr values sent.
    *
    * Unlike the `GPUVerifier__Old`, this method does not use the tree iterator whatosever.  It loops over the surviving values and
    * sends them to the GPU along with a `BinaryTree<T>::st_scaling_factor()` and an internal multiplier, allowing the both the CPU
    * and GPU to mimic the `BinaryTree<T>::for_each_uncovered_value_with_tls()` infinite iteration.
    *
    * Why?  Because it allows this method to send larger work sets to the GPU by asking it to scale all survivor root-values on its
    * own in a loop.  This takes pressure off the CPU iterator and PCIe bus, amortizing the cost of host-device synchronization.
    *
    * @note The `NodeBitmap` assures all values sent in a `ForEachPolicy::PARALLEL` mode will never skip more than one prefix's
    * worth of range if one or more threads issues a premature `ForEachSignal::BREAK`.  This method would normally violate that
    * contract by allowing that range to expand due to the `scales_per_run` factor.  To protect against this, this method will
    * simply issue a `ForEachSignal::CONTINUE` when the `_end_value` is reached, guaranteeing ALL survivor values are buffered.
    * The only downside is the `scales_per_run` will mean verification can go beyond the `_end_value`.  All of this will be
    * accurately reflected in the metrics.
    *
    * @param scales_per_run Controls the number of times a scaling factor is applied to a surviving value on the GPU side.  During
    * a normal `tree.for_each_uncovered_value()` an internal `multiplier` is incremented and applied to a `scaling_factor`, which
    * results in value-ordered enumeration of suviving values using simple arithmetic.  Unfortunately, this requires the CPU to
    * read, store, and then transmit values to the GPU, which also overloads the PCIe bus.  This method skips the tree iterator and
    * iterates over the surviving root values ONLY and tracks multipliers and the scaling factor directly.  By passing these to the
    * GPU, it can do large "runs" of a surviving value's subtree nodes.  This amortizes the CPU work (buffering root values) and
    * the PCIe load.  The only downside is that extremely large `scales_per_run` will keep the GPU busier for longer inside an
    * uninterruptable operation, which means pausing, stopping, or other tear-down might take extra time.
    * @tparam UseIVTable Whether or not to use the max initial value table from `CollatzConstants` to protect from overflow instead
    * of tracking headroom bits in realtime.  Note: when the table is exhausted, the system falls back to headroom bits, making
    * this always safe to send as `true`.
    */
    void run_executor(const VerifierExecutorPolicy& policy) {
        // Sanity check the scaling runs is 1+.
        if (policy.scales_per_run < 1) {
            throw std::invalid_argument("Cannot send a scales-per-run of zero.  Must be 1+.");
        }

        // Find out how much memory is available for the GPU.  Start by assuming the caller sent a fixed byte amount.
        size_t total_buffer_limit = policy.gpu_buffer_limit;

        // If unsent, aim for 90% of the free bytes or total needed (if lesser).
        if (policy.gpu_buffer_limit == 0) {
            size_t free_bytes = 0;
            size_t total_bytes = 0;
            cudaError_t status = cudaMemGetInfo(&free_bytes, &total_bytes);
            if (status != cudaSuccess) {
                throw std::runtime_error(std::string("CUDA Error: ") + cudaGetErrorString(status));
            }
            total_buffer_limit = free_bytes * 9 / 10;  // 90%

            // If the survivor set is smaller than the available buffer space on the GPU, reduce the allocation size needed.
            const size_t uncovered_values_size = this->_tree.get_uncovered_values().cardinality() * sizeof(T);
            if (uncovered_values_size < total_buffer_limit) {
                total_buffer_limit = uncovered_values_size;
            }
        }

        // Use the total buffer limit to decide how large each thread's buffer can be.
        // Add 1-2 elements worth of buffer in case the full survivor set fits in VRAM.  Otherwise, integer truncation might cause
        // edge-cases where a handful are excluded and require a whole round-trip.  It's unlikely, but why risk it?
        const size_t thread_count = static_cast<size_t>(omp_get_max_threads());
        const size_t buffer_limit_per_thread = (total_buffer_limit / thread_count) + (sizeof(T) * 2);

        // Use size_of to determine how many elements that works out to be.  It'll be used by the buffer's constructor.
        const uint64_t elements_per_thread = buffer_limit_per_thread / sizeof(T);

        // Build the TLS objects so element count can be sent.  Otherwise NodeBitmap will default-initialiaze and fail.
        _tls.resize(thread_count);
        for (ThreadStorage& tls : _tls) {
            tls.buffer_ptr = std::make_unique<CudaStreamBuffer<T, GPUVerifierResultData>>(elements_per_thread);
        }

        // Link to to the uncovered values (root survivors).
        const NodeBitmap<T>& uncovered_values = this->_tree.get_uncovered_values();

        // Update the multiplier to use with the scaling factor, which grows by scales_per_run with each loop.
        _base_multiplier = 1;

        // Bump the multiplier if a start value requires it.  Take the maximum value of the tree and subtract it from the start
        // value requested, then divide it by the scaling factor to get the mutliplier.
        if (this->_start_value > uncovered_values.maximum()) {
            _base_multiplier += ((this->_start_value - uncovered_values.maximum()) / _scaling_factor);
        }

        // Create a max multiplier which is the highest (inclusive) multiplier based on the scales_per_run.
        _max_multiplier = _base_multiplier + policy.scales_per_run - 1;

        // Hoist a total scaling factor based on the maximum multiplier the GPU will receive.
        T max_scaling_factor = _scaling_factor * _max_multiplier;

        // Loop through all survivors forever, or until end_value is reached.
        // It's okay if this last_loop is non-atomic.  It's only ever checked outside OMP.
        bool last_loop = false;
        while(! last_loop) {
            uncovered_values.for_each_value_with_tls(ForEachPolicy::PARALLEL, _tls, [&](T& root_survivor_value, ThreadStorage& thread_storage) {
                // Stop if done.  Since a multiplier range is in effect, it breaks the contract of always working within one prefix
                // of a NodeBitmap (2^32 range).  Therefore, don't return BREAK, just flag the atomic for last loop.
                if (this->_end_value > 0 && (max_scaling_factor + root_survivor_value) > this->_end_value) {
                    last_loop = true;
                }

                // If the buffer is full, send it to the GPU for processing.
                if (thread_storage.buffer_ptr->is_full()) {
                    // Call helper to process on the GPU.
                    process_buffer(thread_storage, policy);

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
                thread_storage.buffer_ptr->get_host_data_ptr()[thread_storage.buffer_ptr->use_next_index()] = root_survivor_value;

                // Continue
                return ForEachSignal::CONTINUE;
            });

            // Flush any pending buffers.
            for (ThreadStorage& thread_storage : _tls) {
                if (thread_storage.buffer_ptr->get_next_index() > 0) {
                    process_buffer(thread_storage, policy);
                }
            }

            // Bump the multipliers.
            _base_multiplier += policy.scales_per_run;
            _max_multiplier += policy.scales_per_run;

            // Recompute the max scaling factor.
            max_scaling_factor = _scaling_factor * _max_multiplier;
        }

        // Set state to STOPPED.  Only this thread may do that.
        this->_state.store(VerifierState::STOPPED);

        // Synchronize the timer since it's done doing any work.
        this->sync_timer();
    }



    /// @brief Helper to process a full buffer.
    void process_buffer(ThreadStorage& thread_storage, const VerifierExecutorPolicy& policy) {
        // Mark the buffer READY.
        thread_storage.buffer_ptr->set_state(CudaStreamBufferState::READY);

        // Launch the kernel for the GPU to execute when it's able to.
        if (policy.enable_max_iv_table) {
            thread_storage.buffer_ptr->launch_kernel(launch_gpu_verify_kernel<T, true>, _base_multiplier, _max_multiplier, _scaling_factor);
        } else {
            thread_storage.buffer_ptr->launch_kernel(launch_gpu_verify_kernel<T, false>, _base_multiplier, _max_multiplier, _scaling_factor);
        }

        // Wait for the GPU to mark the buffer PROCESSED.
        thread_storage.buffer_ptr->wait_until_state(CudaStreamBufferState::PROCESSED);

        // Process the results.  Check for overflows and handle them.
        GPUVerifierResultData* host_results_ptr = thread_storage.buffer_ptr->get_host_results_ptr();
        T* host_data_ptr = thread_storage.buffer_ptr->get_host_data_ptr();
        size_t scales_ran = _max_multiplier - _base_multiplier + 1;
        if (host_results_ptr->overflow_count > 0) {
            // Had at least one overflow.  Check if we contained them.
            if (host_results_ptr->overflow_exceeded) {
                // Ran out of overflow slots.  Must reprocess the whole batch on CPU.
                for (uint64_t i = 0; i < thread_storage.buffer_ptr->get_next_index(); i++) {
                    T base_value = *(host_data_ptr + i);
                    for (size_t multiplier = _base_multiplier; multiplier <= _max_multiplier; multiplier++) {
                        T initial_value = base_value + (multiplier * _scaling_factor);
                        if (! Collatz<T>::st_verify(initial_value, initial_value)) {
                            throw std::logic_error("Failed to verify value: " + to_string_any(initial_value));
                        }
                    }
                }
                // Update the metrics for this, including the overflow-exceeded counter.
                this->_published_metrics.gpu_overflow_buffer_exceeded.fetch_add(1);
                this->_published_metrics.gpu_overflows_processed.fetch_add(((thread_storage.buffer_ptr->get_next_index() + 1) * scales_ran));
            } else {
                // Contained the overflow.  Process only the offenders on CPU.
                for (uint64_t i = 0; i < host_results_ptr->overflow_count; i++) {
                    T base_value = *(host_data_ptr + host_results_ptr->overflow_indexes[i]);
                    for (size_t multiplier = _base_multiplier; multiplier <= _max_multiplier; multiplier++) {
                        T initial_value = base_value + (multiplier * _scaling_factor);
                        if (!Collatz<T>::st_verify(initial_value, initial_value)) {
                            throw std::logic_error("Failed to verify value: " + to_string_any(initial_value));
                        }
                    }
                }
                // Update the metrics for this.
                this->_published_metrics.gpu_overflows_processed.fetch_add(host_results_ptr->overflow_count * scales_ran);
            }
        }

        // Reset the results.  They'll be sync'd as-such to the device.
        host_results_ptr->reset();

        // Update metrics.
        this->_published_metrics.nodes_verified_atomic.fetch_add(thread_storage.buffer_ptr->get_next_index() * scales_ran);
        this->_published_metrics.gpu_kernel_launches.fetch_add(1);

        // Recycle the buffer.  This resets counters and sets state to FILLING.
        thread_storage.buffer_ptr->recycle();
    }

};
