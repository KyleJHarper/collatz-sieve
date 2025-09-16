#pragma once
#include <cstddef>
#include <stdint.h>
#include <cuda_runtime.h>


template<typename T>
class CollatzPeakRunner;

template<typename T>
CollatzPeakRunner<T>* create_runner(T* host_buffer, size_t buffer_size, T* unified_base_initial_value, int* unified_overflow_index, int* unified_failing_index);

template<typename T>
void destroy_runner(CollatzPeakRunner<T>* runner);

template<typename T>
void compute_peak(CollatzPeakRunner<T>* runner, size_t bit, bool skip_after_hwm);

template<typename T>
void run_single_iv(
    T iv,
    bool skip_after_hwm,
    T* h_out_peak,
    int* h_out_overflow_flag,
    T* h_out_max3xp1 // returns the get_max_3xp1<uint64_t>() value for sanity
);
