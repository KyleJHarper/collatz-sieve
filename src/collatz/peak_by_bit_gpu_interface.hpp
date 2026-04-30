#pragma once
#include <cstddef>
#include <stdint.h>

#ifdef HAVE_CUDA
#include <cuda_runtime.h>
#endif


/// @brief Expose the CollatzPeakRunner in the .cu file.
template<typename T>
class CollatzPeakRunner;



/// @brief Expose the `create_runner` method in the .cu file.
template<typename T>
CollatzPeakRunner<T>* create_runner(T* host_buffer, size_t buffer_size, T* unified_base_initial_value, int* unified_overflow_index, int* unified_failing_index);



/// @brief Expose the `destroy_runner()` method in the .cu file.
template<typename T>
void destroy_runner(CollatzPeakRunner<T>* runner);



/// @brief Expose the find_max_iv_for_bit_gpu()` method in the .cu file.
template<typename T>
void find_max_iv_for_bit_gpu(CollatzPeakRunner<T>* runner, size_t bit);
