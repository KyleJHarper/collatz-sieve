#include <cuda_runtime_api.h>
#include <gmp.h>
#include <gmpxx.h>
#include "collatz/collatz.hpp"

int main() {
    std::cout << Collatz<uint64_t>::st_get_step_count_fast(3) << std::endl;
    std::cout << Collatz<uint64_t>::st_get_peak_fast(3) << std::endl;
    std::cout << Collatz<uint64_t>::st_get_peak_fast(7) << std::endl;

}