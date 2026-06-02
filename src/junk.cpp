#include <iostream>
#include "collatz/binary_tree.hpp"
// #include "collatz/for_each_policy.hpp"
// #include "collatz/node_bitmap.hpp"
#include <chrono>
#include <ctime>
#include <omp.h>
#include <zstd.h>


inline void log_time(const char* message)
{
    using namespace std::chrono;

    auto now = system_clock::now();
    auto now_time = system_clock::to_time_t(now);

    auto ms = duration_cast<milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm bt{};
    localtime_r(&now_time, &bt);

    std::cout
        << std::put_time(&bt, "%H:%M:%S")
        << '.'
        << std::setw(3)
        << std::setfill('0')
        << ms.count()
        << " - "
        << message
        << '\n';
}
inline void log_time(const std::string& message) {
    log_time(message.c_str());
}



int main(int argc, char** argv) {

    // using my_t = uint64_t;
    // using my_t = uint128_t;

    if (argc < 2) {
        std::cerr << "You must pass the number of levels as arg1" << std::endl;
        return 1;
    }

    int levels = atol(argv[1]);

    BinaryTree<uint64_t, BinaryTreeImplicitImpl<uint64_t>> implicit_tree(levels);
    BinaryTree<uint64_t, BinaryTreeMaterializedImpl<uint64_t>> materialized_tree(levels);

    size_t count = 0;
    implicit_tree.generate_value_map();
    implicit_tree.for_each_uncovered_value(ForEachPolicy::SERIAL, [&](const uint64_t& value) {
        std::cout << value << std::endl;
        count++;
        if (count > 100) {
            return ForEachSignal::BREAK;
        }
        return ForEachSignal::CONTINUE;
    });

    count = 0;
    materialized_tree.generate_value_map();
    materialized_tree.for_each_uncovered_value(ForEachPolicy::SERIAL, [&](const uint64_t& value) {
        std::cout << value << std::endl;
        count++;
        if (count > 100) {
            return ForEachSignal::BREAK;
        }
        return ForEachSignal::CONTINUE;
    });

}
