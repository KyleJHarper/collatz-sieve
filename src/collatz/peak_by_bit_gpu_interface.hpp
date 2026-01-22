#pragma once
#include <cstddef>
#include <stdint.h>

#ifdef HAVE_CUDA
#include <cuda_runtime.h>
#endif


template<typename T>
class CollatzPeakRunner;

template<typename T>
CollatzPeakRunner<T>* create_runner(T* host_buffer, size_t buffer_size, T* unified_base_initial_value, int* unified_overflow_index, int* unified_failing_index);

template<typename T>
void destroy_runner(CollatzPeakRunner<T>* runner);

template<typename T>
void find_max_iv_for_bit_gpu(CollatzPeakRunner<T>* runner, size_t bit);
