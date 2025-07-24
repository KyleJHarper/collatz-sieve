#include <climits>
#include <cstdio>
#include <cstdlib>
#include <stdint.h>
#include <inttypes.h>
#include <iostream>
#include "collatz/collatz.hpp"
// #include "experiments/experiment_a1.hpp"


int main(int argc, char **argv) {

    unsigned long long initial_value = 0;
    if (argc < 2) {
        std::cerr << "You must send a number as arg1." << std::endl;
        return 1;
    }
    try {
        initial_value = std::strtoull(argv[1], NULL, 10);
    } catch (const std::invalid_argument& e) {
        std::cerr << "Invalid argument: " << e.what() << std::endl;
        return 1;
    } catch (const std::out_of_range& e) {
        std::cerr << "Out of range: " << e.what() << std::endl;
        return 1;
    }

    //uint initial_value = atoi(argv[1]);
    Collatz c = Collatz<uint>(initial_value);
    std::cout << "Value is: " << c << std::endl;
    std::cout << c.get_sequence().back() << std::endl;
    std::cout << "Sequence is: " << c.get_sequence_string() << std::endl;
    std::cout << "OE pattern is: " << c.get_oe_pattern() << std::endl;
    std::cout << "HWM index is: " << c.get_hwm_index() << std::endl;
    std::cout << "Stop count: " << c.get_stop_count() << std::endl;
    std::cout << "Peak value: " << c.get_peak_value() << std::endl;

    return 0;
}
