#include <cstdio>
#include <stdint.h>
#include <inttypes.h>
#include <iostream>
#include "collatz/collatz.hpp"
// #include "experiments/experiment_a1.hpp"


int main(int argc, char **argv) {

    if (argc < 2) {
        std::cerr << "You must send a number as arg1." << std::endl;
        return 1;
    }
    uint initial_value = atoi(argv[1]);
    Collatz c = Collatz<uint>(initial_value);
    std::cout << "Value is: " << c << std::endl;
    std::cout << c.get_sequence().back() << std::endl;
    std::cout << "Sequence is: " << c.get_sequence_string() << std::endl;
    std::cout << "OE pattern is: " << c.get_oe_pattern() << std::endl;
    std::cout << "HWM index is: " << c.get_hwm_index() << std::endl;
    std::cout << "Stop count: " << c.get_stop_count() << std::endl;

    return 0;
}
