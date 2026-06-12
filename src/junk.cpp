#include "collatz/collatz.hpp"
#include <stdexcept>
#include <iostream>


int main(int argc, char** argv) {

    if (argc < 2) {
        std::cerr << "You must specify count to verify as arg1." << std::endl;
        return 1;
    }

    uint64_t count = std::atoll(argv[1]);
    for (uint64_t i = 2; i < count; i++) {
        if (! Collatz<uint64_t>::st_verify_to_hwm(i)) {
            throw std::runtime_error("What...?");
        }
    }

}
