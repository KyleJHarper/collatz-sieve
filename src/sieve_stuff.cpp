#include <climits>
#include <gmpxx.h>
#include <unistd.h>
#include "collatz/sieve.hpp"


int main() {
    // CLI::App options("Wheee");
    // size_t levels;
    // options.add_option("-l,--levels", levels, "How many levels to build the tree.")->default_val(3);
    // CLI11_PARSE(options, argc, argv);

    // SieveOptions opts{};
    // opts.pool_size = 32;
    // Sieve<mpz_class> sieve(levels, opts);
    // mpz_class v;
    // std::vector<mpz_class> bulk(5000);
    // for (size_t i = 0; i < 1000000; i+=bulk.size()) {
    //     sieve.next(bulk);
    // }
    // std::cout << "Refills: " << sieve.get_pool_refills() << std::endl;
    // std::cout << "Premature Refills: " << sieve.get_pool_premature_refills() << std::endl;
    // std::cout << "Last value: " << bulk.back() << std::endl;


    return 0;
}
