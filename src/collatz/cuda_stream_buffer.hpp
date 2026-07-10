#pragma once
#include "concepts.hpp"
#include <cuda_runtime.h>
#include <cstdint>
#include <features.h>
#include <stdexcept>
#include <atomic>



/// @brief States of a buffer for the `CudaStreamBuffer` class below.
enum class CudaStreamBufferState : uint32_t {
    FILLING = 1,     ///< Buffer is filling up.
    READY = 2,       ///< Buffer is ready.  CPU has filled it.  GPU may process it.
    PROCESSED = 3,   ///< Buffer is processed.  GPU is done.  CPU may read/reuse/refill/etc.
};




template<FixedWidthIntegral T, typename ResultData>
class CudaStreamBuffer {
    private:
    /// @brief Number of elements in each buffer, max.
    const uint64_t _element_limit;
    /// @brief Bytes occupied by each buffer.
    const uint64_t _bytes;
    /// @brief The next element index.
    uint64_t _next_index = 0;

    /// @brief The host buffer.
    T* _host_data = nullptr;
    /// @brief The device (GPU) buffer.
    T* _device_data = nullptr;

    /// @brief The host result data object.
    ResultData* _host_results = nullptr;
    /// @brief The device result data object.
    ResultData* _device_results = nullptr;

    /// @brief The current state of the buffer.  Only the host code modifies this, by help of `cudaLaunchHostFunc`.
    std::atomic<CudaStreamBufferState> _state = CudaStreamBufferState::FILLING;

    /// @brief Dedicated CUDA stream for synchronizing.
    cudaStream_t _stream = nullptr;



    public:
    /// @name Lifecycle Management
    /// @{

    /// @brief No default constructor allowed.  Must pass element count.
    CudaStreamBuffer() = delete;


    /// @brief Constructor takes element count and deduces bytes required.
    explicit CudaStreamBuffer(uint64_t element_count)
        : _element_limit(element_count)
        , _bytes(element_count * sizeof(T))
    {
        // Allocate the pinned host memory buffer.
        if (cudaMallocHost((void**)&_host_data, _bytes) != cudaSuccess) {
            throw std::runtime_error("Failed to allocate host pinned data");
        }

        // Allocate the pinned host results data.
        if (cudaMallocHost((void**)&_host_results, sizeof(ResultData)) != cudaSuccess) {
            cudaFreeHost(_host_data);
            throw std::runtime_error("Failed to allocate host pinned result data");
        }

        // Allocate the GPU buffer.
        if (cudaMalloc((void**)&_device_data, _bytes) != cudaSuccess) {
            cudaFreeHost(_host_data);
            cudaFreeHost(_host_results);
            throw std::runtime_error("Failed to allocate device data");
        }

        // Allocate the GPU results data.
        if (cudaMalloc((void**)&_device_results, sizeof(ResultData)) != cudaSuccess) {
            cudaFreeHost(_host_data);
            cudaFreeHost(_host_results);
            cudaFree(_device_data);
            throw std::runtime_error("Failed to allocate device data");
        }

        // Build the non-blocking stream.
        if (cudaStreamCreateWithFlags(&_stream, cudaStreamNonBlocking) != cudaSuccess) {
            cudaFreeHost(_host_data);
            cudaFreeHost(_host_results);
            cudaFree(_device_data);
            cudaFree(_device_results);
            throw std::runtime_error("Failed to create CUDA stream");
        }
    }



    /// @brief Destructor will unwind and free all memory.
    ~CudaStreamBuffer() {
        if (_stream) { cudaStreamDestroy(_stream); }
        if (_host_data) { cudaFreeHost(_host_data); }
        if (_host_results) { cudaFreeHost(_host_results); }
        if (_device_data) { cudaFree(_device_data); }
        if (_device_results) { cudaFree(_device_results); }
    }



    /// @brief Disallow copying.
    CudaStreamBuffer(const CudaStreamBuffer&) = delete;
    /// @brief Disallow copying.
    CudaStreamBuffer& operator=(const CudaStreamBuffer&) = delete;

    /// @}



    /// @name Accessors
    /// @{

    /// @brief Get the host pointer to the buffer.
    T* get_host_data_ptr() const { return _host_data; }



    /// @brief Get the host pointer to the results.
    ResultData* get_host_results_ptr() const { return _host_results; }



    /// @brief Get the number of elements in the buffer.
    uint64_t get_element_count() const { return _element_limit; }



    /// @brief Syntatic sugar to return if the buffer is "full", meaning the elements buffered count matches the element limit.
    bool is_full() const { return _next_index >= _element_limit; }



    /// @brief Returns the next available index in the buffer without changing or incrementing it.
    uint64_t get_next_index() const { return _next_index; }
    /// @brief Returns the next available index in the buffer, and increments internal counters.
    uint64_t use_next_index() { return _next_index++; }



    /// @brief Returns the current state.
    CudaStreamBufferState get_state(std::memory_order m = std::memory_order_relaxed) const { return _state.load(m); }



    /// @brief Sets the current state.
    void set_state(CudaStreamBufferState state) {
        _state.store(state, std::memory_order_release);
        _state.notify_all();
    }

    /// @}



    /// @brief Resets the counters and sets the buffer to FILLING state.
    void recycle() {
        _next_index = 0;
        set_state(CudaStreamBufferState::FILLING);
    }



    /// @brief Waits until the buffer enters a given state.
    void wait_until_state(CudaStreamBufferState desired_state) {
        while(true) {
            CudaStreamBufferState current_state = _state.load(std::memory_order_acquire);
            if (current_state == desired_state) {
                return;
            }
            _state.wait(current_state, std::memory_order_acquire);
        }
    }



    /// @brief Synchronize the host data to the device.
    void sync_host_data_to_device() {
        cudaMemcpyAsync(_device_data, _host_data, _bytes, cudaMemcpyHostToDevice, _stream);
    }



    /// @brief Synchronize the host results to the device.
    void sync_host_results_to_device() {
        cudaMemcpyAsync(_device_results, _host_results, sizeof(ResultData), cudaMemcpyHostToDevice, _stream);
    }



    /// @brief Synchronize the device data to the host.
    void sync_device_data_to_host() {
        cudaMemcpyAsync(_host_data, _device_data, _bytes, cudaMemcpyDeviceToHost, _stream);
    }



    /// @brief Synchronize the device results to the host.
    void sync_device_results_to_host() {
        cudaMemcpyAsync(_host_results, _device_results, sizeof(ResultData), cudaMemcpyDeviceToHost, _stream);
    }



    /**
    * @brief Takes a kernel call reference, synchronizes buffers from host-to-device, and then executes the kernel.
    *
    * The kernel passed in MUST have this signature: `(T*, ResultData*, size_t, cudaStream_t)`
    *
    * Where:
    *   * `T*` will be the device buffer from this class.
    *   * `ResultData*` will be the device result memory for your kernel to shared results.
    *   * `size_t` will be the element count for your kernel to use with loops/boundaries.
    *   * `cudaStream_t` will be the pre-established stream for host<->device synchronization.
    *
    * @note This method injects a state change after the kernel.  Caller should NOT set state directly.
    */
    template<typename Kernel>
    void launch_kernel(Kernel&& kernel_func, size_t base_multiplier = 1, size_t max_multiplier = 1, T scaling_factor = 0) {
        // Make sure this buffer is READY.  Otherwise, error.
        if (get_state() != CudaStreamBufferState::READY) {
            throw std::runtime_error("A buffer must be in the READY state before launching a kernel.");
        }

        // Synchronize data from host to device.
        sync_host_data_to_device();
        sync_host_results_to_device();

        // Launch the user-provided kernel wrapper.
        kernel_func(_device_data, _device_results, _next_index, _stream, base_multiplier, max_multiplier, scaling_factor);

        // Sync the results back.  Don't need the device buffer (data).
        sync_device_results_to_host();

        // Inject a state = PROCESSED flag operation directly into the stream.  Guarantees ordering after GPU threads are done.
        cudaLaunchHostFunc(_stream, [](void* p) {
            static_cast<CudaStreamBuffer*>(p)->set_state(CudaStreamBufferState::PROCESSED);
        }, this);
    }

};
