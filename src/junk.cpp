#include <cuda_runtime_api.h>
#include <gmp.h>
#include <gmpxx.h>
#include "collatz/collatz.hpp"
#include "collatz/collatz_gpu_interface.hpp"

int main() {
    typedef unsigned __int128 my_type;
    // typedef uint64_t my_type;
    my_type ivs[] = {29717743ULL, 80049391ULL };
    for (my_type iv : ivs) {
        // CPU peak
        Collatz<my_type> collatz(iv, true, true);
        my_type cpu_peak = collatz.get_peak_value();

        // GPU peak
        my_type gpu_peak = 0;
        int gpu_overflow_flag = 0;
        my_type gpu_max_3xp1 = 0;
        run_single_iv(iv, true, &gpu_peak, &gpu_overflow_flag, &gpu_max_3xp1);

        std::cout << "IV=" << to_string_any(iv) << std::endl;
        std::cout << "  CPU_peak: " << to_string_any(cpu_peak) << std::endl;
        std::cout << "  GPU_peak: " << to_string_any(gpu_peak) << std::endl;
        std::cout << "  CPU_overflow: " << "0" << std::endl;
        std::cout << "  GPU_overflow: " << to_string_any(gpu_overflow_flag) << std::endl;
        std::cout << "  CPU_max3xp1: " << to_string_any((my_type)CollatzConstants::get_max_3xp1<my_type>()) << std::endl;
        std::cout << "  GPU_max3xp1: " << to_string_any(gpu_max_3xp1) << std::endl;
    }
}