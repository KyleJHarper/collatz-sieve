#include <stdint.h>
#include <iostream>
#include <sys/types.h>
#include "collatz/collatz.hpp"
#include "gmpxx.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "You must send a number as arg1." << std::endl;
        return 1;
    }

    mpz_class initial_value = 0;
    initial_value.set_str(argv[1], 10);
    Collatz c = Collatz<mpz_class>(initial_value);

    // uint64_t initial_value = std::atoll(argv[1]);
    // Collatz c = Collatz<uint64_t>(initial_value);

    std::cout << "Value is: " << c << std::endl;
    std::cout << "Sequence is: " << c.get_sequence_string() << std::endl;
    std::cout << "OE pattern is: " << c.get_oe_pattern_string() << std::endl;
    std::cout << "HWM index is: " << c.get_hwm_index() << std::endl;
    std::cout << "Stop count: " << c.get_stop_count() << std::endl;
    std::cout << "Peak value: " << c.get_peak_value() << std::endl;
    std::cout << "Deep size is: " << c.deep_size() << std::endl;

    return 0;
}
