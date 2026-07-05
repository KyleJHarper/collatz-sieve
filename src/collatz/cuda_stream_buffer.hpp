#pragma once
#include "abi.hpp"
#include "concepts.hpp"
#include <cuda_runtime.h>
#include <cstdint>
#include <features.h>
#include <stdexcept>
#include <variant>
#include <atomic>



/// @brief States of a buffer for the `CudaStreamBuffer` class below.
enum class CudaStreamBufferState : uint32_t {
    EMPTY = 0,       ///< Buffer is empty (new).  CPU or similar should fill it.
    FILLING = 1,     ///< Buffer is filling up.  One or more threads may be held waiting for it to finish.
    FILLED = 2,      ///< Buffer is full and awaiting a coordinator to change it to READY.
    READY = 3,       ///< Buffer is ready.  CPU has filled it.  GPU may process it.
    PROCESSED = 4,   ///< Buffer is processed.  GPU is done.  CPU may read/reuse/refill/etc.
};



struct alignas(ABI::CACHE_LINE_SIZE) CudaStreamBufferChunk {
    /// @brief The index of the chunk inside a buffer, used when considering if all chunks have been filled or not.
    uint64_t index = 0;

    /// @brief The start index within the buffer managed by this chunk, inclusive/half-open-range: [start - end).
    uint64_t buffer_start = 0;

    /// @brief The current index within the buffer managed by this chunk that a thread is filling.
    uint64_t buffer_index = 0;

    /// @brief The end index within the buffer managed by this chunk, exclusive/half-open-range: [start - end).
    uint64_t buffer_end = 0;

    /// @brief Compares the `buffer_index` to the `buffer_end` to decide if this chunk is finished.
    inline bool finished() const { return buffer_index >= buffer_end; }

    /// @brief Returns true if the chunk has at least one element: end - start.
    inline bool empty() const { return buffer_end == buffer_start; }
};




template<FixedWidthIntegral T, typename ResultData = std::monostate>
class CudaStreamBuffer {
    private:
    /// @brief Number of elements in each buffer.
    const uint64_t _element_count;
    /// @brief Bytes occupied by each buffer.
    const uint64_t _bytes;

    /// @brief The host buffer.
    T* _host_data = nullptr;
    /// @brief The device (GPU) buffer.
    T* _device_data = nullptr;

    /// @brief The host result data object.
    ResultData* _host_results = nullptr;
    /// @brief The device result data object.
    ResultData* _device_results = nullptr;

    /// @brief The current state of the buffer.  Only the host code modifies this, by help of `cudaLaunchHostFunc`.
    std::atomic<CudaStreamBufferState> _state = CudaStreamBufferState::EMPTY;

    /// @brief Dedicated CUDA stream for synchronizing.
    cudaStream_t _stream = nullptr;

    /// @brief Chunk size for callers who want to chunk the data, usually for parallel access to buffered memory.  This only
    /// controls how many values threads load into a buffer before requesting another chunk.  The primary advantage is taking
    /// pressure of the `std:atomic` feeding the `chunk` by reducing `fetch_add()` calls.  These calls are already cheap, so this
    /// value doesn't need to be huge.
    const uint64_t _chunk_size;

    /// @brief The next chunk to issue.  This is an index to create a base value for chunks.
    std::atomic<uint64_t> _next_chunk_index = 0;

    /// @brief The number of chunks filled thus far.
    std::atomic<uint64_t> _chunks_filled = 0;

    /// @brief The number of chunks expected for the buffer based on element count and chunk size.
    const uint64_t _chunks_expected;



    public:
    /// @name Lifecycle Management
    /// @{

    /// @brief No default constructor allowed.  Must pass element count.
    CudaStreamBuffer() = delete;


    /// @brief Constructor takes element count and deduces bytes required.
    explicit CudaStreamBuffer(uint64_t element_count, uint64_t chunk_size = 4096)
        : _element_count(element_count)
        , _bytes(element_count * sizeof(T))
        , _chunk_size(chunk_size)
        , _chunks_expected((element_count + chunk_size - 1) / chunk_size)
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
    uint64_t get_element_count() const { return _element_count; }



    /// @brief Returns the current state.
    CudaStreamBufferState get_state(std::memory_order m = std::memory_order_relaxed) const { return _state.load(m); }



    /// @brief Sets the current state.
    void set_state(CudaStreamBufferState state) {
        _state.store(state, std::memory_order_release);
        _state.notify_all();
    }

    /// @}



    /// @name Chunking
    /// @{

    /// Updates caller's chunk with the next available.  Could be out of bounds.  Caller must verify from return.
    /// Returns true if valid.  False otherwise.
    bool get_next_chunk(CudaStreamBufferChunk& chunk) {
        chunk.index = _next_chunk_index.fetch_add(1, std::memory_order_relaxed);
        chunk.buffer_start = chunk.index * _chunk_size;
        chunk.buffer_index = chunk.buffer_start;
        chunk.buffer_end = chunk.buffer_index + _chunk_size;
        if (chunk.buffer_end > _element_count) {
            chunk.buffer_end = _element_count;
        }

        // If it's a valid chunk of the buffer, ensure the state is set to FILLING and return true.
        if (chunk.index < _chunks_expected) {
            CudaStreamBufferState expected = CudaStreamBufferState::EMPTY;
            _state.compare_exchange_strong(expected, CudaStreamBufferState::FILLING, std::memory_order_release);
            return true;
        }

        // Chunk index was too large.  Not a real space.  Return false.
        return false;
    }



    /// @brief Returns a chunk from a worker, updating necessary internal trackers.
    void return_chunk(const CudaStreamBufferChunk& chunk) {
        // Increment the chunks filled counter if the chunk was real (aka: non-zero).
        if (chunk.empty() == false) {
            uint64_t filled = _chunks_filled.fetch_add(1, std::memory_order_release);
            if (filled + 1 == _chunks_expected) {
                set_state(CudaStreamBufferState::FILLED);
            }
        }
    }



    /// @brief Return the current number of chunks filled.
    uint64_t get_chunks_filled() const {
        return _chunks_filled.load(std::memory_order_acquire);
    }



    /// @brief Resets the counters related to chunking.  Pushing index to 0 and "filled" to 0.  Then sets state to `EMPTY`.
    void recycle() {
        _next_chunk_index.store(0, std::memory_order_relaxed);
        _chunks_filled.store(0, std::memory_order_relaxed);
        set_state(CudaStreamBufferState::EMPTY);
    }

    /// @}



    /// @name State Handling
    /// @{

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

    /// @}



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
    * @brief Takes a kernel call reference (ptr), synchronizes buffers from host-to-device, and then executes the kernel.
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
    void launch_kernel(Kernel&& kernel_func) {
        // Make sure this buffer is READY.  Otherwise, error.
        if (get_state() != CudaStreamBufferState::READY) {
            throw std::runtime_error("A buffer must be in the READY state before launching a kernel.");
        }

        // Synchronize data from host to device.
        sync_host_data_to_device();
        sync_host_results_to_device();

        // Launch the user-provided kernel wrapper.
        kernel_func(_device_data, _device_results, _element_count, _stream);

        // Sync the results back.  Don't need the device buffer (data).
        sync_device_results_to_host();

        // Inject a state = PROCESSED flag operation directly into the stream.  Guarantees ordering after GPU threads are done.
        cudaLaunchHostFunc(_stream, [](void* p) {
            static_cast<CudaStreamBuffer*>(p)->set_state(CudaStreamBufferState::PROCESSED);
        }, this);
    }

};
