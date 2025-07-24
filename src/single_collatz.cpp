#include <iostream>
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
    std::cout << "Value is: " << c << std::endl;
    std::cout << "Sequence is: " << c.get_sequence_string() << std::endl;
    std::cout << "OE pattern is: " << c.get_oe_pattern() << std::endl;
    std::cout << "HWM index is: " << c.get_hwm_index() << std::endl;
    std::cout << "Stop count: " << c.get_stop_count() << std::endl;
    std::cout << "Peak value: " << c.get_peak_value() << std::endl;

    mpf_class my_mpf = 0;
    mpz_class one = 1;
    mpz_class four = 4;
    mpf_class one_mpf = one;
    my_mpf = one_mpf / four;
    std::cout << "my_mpf: " << my_mpf << std::endl;
    return 0;
}
