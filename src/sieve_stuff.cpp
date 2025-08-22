#include <climits>
#include <cstddef>
#include <gmpxx.h>
#include <string>
#include <unistd.h>
#include "include/CLI11.hpp"
#include "collatz/sieve.hpp"


int main(int argc, char **argv) {
    CLI::App options("Wheee");
    size_t levels;
    options.add_option("-l,--levels", levels, "How many levels to build the tree.")->default_val(3);
    CLI11_PARSE(options, argc, argv);

    Sieve<mpz_class> sieve(levels);
    for (size_t i = 0; i < 100; i++) {
        std::cout << sieve.next() << std::endl;
    }

    return 0;
}
