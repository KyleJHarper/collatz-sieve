#pragma once


#ifdef HAVE_CUDA
#include <cuda_runtime_api.h>


bool can_use_gpu() {
    int count = 0;
    if (cudaGetDeviceCount(&count) != cudaSuccess || count == 0) {
        return false;
    }
    return true;
}
#else

bool can_use_gpu() { return false; }

#endif
