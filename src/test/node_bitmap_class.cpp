#include <cassert>
#include <cstdint>
#include <iostream>
#include <gmpxx.h>
#include "../collatz/node_bitmap.hpp"


template<AnySupportedIntegral T>
void test_node_bitmap_basics() {
    NodeBitmap<T> bitmap;
    assert(bitmap.contains(42) == false);
    bitmap.add(42);
    assert(bitmap.contains(42) == true);
    bitmap.remove(42);
    assert(bitmap.contains(42) == false);
    bitmap.add(4242);
    assert(bitmap.contains(4242) == true);
    bitmap.optimize();
    assert(bitmap.contains(4242) == true);
    bitmap.clear();
    assert(bitmap.contains(4242) == false);
}



template<AnySupportedIntegral T>
void test_node_bitmap_bulk_insert() {
    NodeBitmap<T> bitmap;
    T start = 42;
    T end = 4242;
    bitmap.add_range(start, end);
    for (T i = start; i < end; i++) {
        assert(bitmap.contains(i));
    }
    NodeBitmap<T> bitmap2;
    bitmap2.add_range_closed(start, end);
    for (T i = start; i <= end; i++) {
        assert(bitmap2.contains(i));
    }
}



template<AnySupportedIntegral T>
void test_node_bitmap_prefix_boundary() {
    NodeBitmap<T> bitmap;
    T value_bigger_than_suffix = BitmapKeyTraits<T>::SUFFIX_MAX + 1000;
    bitmap.add(value_bigger_than_suffix);
    assert(bitmap.contains(value_bigger_than_suffix) == true);
}




//
// Wrapper to run all tests.
//
template<AnySupportedIntegral T>
void run_all() {
    std::cout << "test_node_bitmap_basics() ..." << std::flush;
    test_node_bitmap_basics<T>();
    std::cout << " passed.\n";

    std::cout << "test_node_bitmap_bulk_insert() ..." << std::flush;
    test_node_bitmap_bulk_insert<T>();
    std::cout << " passed.\n";

    std::cout << "test_node_bitmap_prefix_boundary() ..." << std::flush;
    test_node_bitmap_prefix_boundary<T>();
    std::cout << " passed.\n";

}



int main() {
    std::cout << "=====================" << std::endl;
    std::cout << "NodeBitmap Tests" << std::endl;
    std::cout << "=====================" << std::endl;

    std::cout << "Performing tests with uint64_t." << std::endl;
    run_all<uint64_t>();

    std::cout << "Performing tests with uint128_t." << std::endl;
    run_all<uint128_t>();

    std::cout << "Performing tests with mpz_class." << std::endl;
    run_all<mpz_class>();

    std::cout << "All NodeBitmap<T> tests passed.\n";
    return 0;
}
