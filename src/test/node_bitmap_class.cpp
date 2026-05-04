#include <cassert>
#include <cstdint>
#include <iostream>
#include <gmpxx.h>
#include "../collatz/node_bitmap.hpp"


template<AnySupportedIntegral T>
void test_node_bitmap_basics() {
    NodeBitmap<T> bitmap;
    assert(bitmap.cardinality() == 0);
    assert(bitmap.contains(42) == false);
    bitmap.add(42);
    assert(bitmap.cardinality() == 1);
    assert(bitmap.contains(42) == true);
    bitmap.remove(42);
    assert(bitmap.cardinality() == 0);
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
    assert(bitmap.cardinality() == 0);
    bitmap.add(value_bigger_than_suffix);
    assert(bitmap.cardinality() == 1);
    assert(bitmap.contains(value_bigger_than_suffix) == true);
    bitmap.add(42);
    assert(bitmap.cardinality() == 2);
    assert(bitmap.contains(42));
    bitmap.clear();
    assert(bitmap.cardinality() == 0);
    //
    // Now store a lot of bigger prefix stuff.
    bitmap.clear();
    size_t count = 100000;
    size_t low_bit = size_t(std::log2(count)) + 1;  // Needs to be 2^x > count or we'll get duplicates.
    size_t high_bit = 64;
    for (size_t bits = low_bit; bits < high_bit; bits++) {
        T value = T(1) << bits;
        for (size_t i = 0; i < count; i++) {
            bitmap.add(value + i);
        }
    }
    assert(bitmap.cardinality() == (high_bit - low_bit) * count);
    //
    // Go beyond 64 bits when possible.
    bitmap.clear();
    if constexpr(FixedWidthIntegral<T> && sizeof(T) == 16) {
        low_bit = 64;
        high_bit = 128;
        for (size_t bits = low_bit; bits < high_bit; bits++) {
            T value = T(1) << bits;
            for (size_t i = 0; i < count; i++) {
                bitmap.add(value + i);
            }
        }
        assert(bitmap.cardinality() == (high_bit - low_bit) * count);
    }
    if constexpr(GMPIntegral<T>) {
        low_bit = 64;
        high_bit = 256;
        for (size_t bits = low_bit; bits < high_bit; bits++) {
            T value = T(1) << bits;
            for (size_t i = 0; i < count; i++) {
                bitmap.add(value + i);
            }
        }
        assert(bitmap.cardinality() == (high_bit - low_bit) * count);
    }
}



template<AnySupportedIntegral T>
void test_node_bitmap_for_each_value() {
    NodeBitmap<T> bitmap;
    T start = 42;
    T end = 4242;
    bitmap.add_range(start, end);
    T current = start;
    bitmap.for_each_value([&](const T& value){
        assert(value == current);
        current++;
    });
    assert(current == end + 1);
}


//
// Merging
//
template<AnySupportedIntegral T>
void test_node_bitmap_merge() {
    NodeBitmap<T> bitmap_src;
    NodeBitmap<T> bitmap_dst;
    bitmap_src.add(1);
    bitmap_src.add(2);
    bitmap_src.add(3);
    assert(bitmap_src.cardinality() == 3);
    assert(bitmap_dst.cardinality() == 0);
    bitmap_dst.merge(bitmap_src);
    assert(bitmap_dst.cardinality() == 3);
    assert(bitmap_dst.contains(1));
    assert(bitmap_dst.contains(2));
    assert(bitmap_dst.contains(3));
    assert(bitmap_dst.contains(4) == false);
    //
    // Now redo it with pre-existing values.
    bitmap_dst.clear();
    assert(bitmap_dst.cardinality() == 0);
    bitmap_dst.add(42);
    assert(bitmap_dst.contains(42));
    bitmap_dst.merge(bitmap_src);
    assert(bitmap_dst.cardinality() == 4);
    assert(bitmap_dst.contains(1));
    assert(bitmap_dst.contains(2));
    assert(bitmap_dst.contains(3));
    assert(bitmap_dst.contains(4) == false);
    assert(bitmap_dst.contains(42));
    //
    // The operator|=() should work identially.
    bitmap_src.clear();
    bitmap_dst.clear();
    bitmap_src.add(1);
    bitmap_src.add(2);
    bitmap_src.add(3);
    assert(bitmap_src.cardinality() == 3);
    assert(bitmap_dst.cardinality() == 0);
    bitmap_dst |= bitmap_src;
    assert(bitmap_dst.cardinality() == 3);
    assert(bitmap_dst.contains(1));
    assert(bitmap_dst.contains(2));
    assert(bitmap_dst.contains(3));
    assert(bitmap_dst.contains(4) == false);
    //
    // Now redo it with pre-existing values.
    bitmap_dst.clear();
    assert(bitmap_dst.cardinality() == 0);
    bitmap_dst.add(42);
    assert(bitmap_dst.contains(42));
    bitmap_dst |= bitmap_src;
    assert(bitmap_dst.cardinality() == 4);
    assert(bitmap_dst.contains(1));
    assert(bitmap_dst.contains(2));
    assert(bitmap_dst.contains(3));
    assert(bitmap_dst.contains(4) == false);
    assert(bitmap_dst.contains(42));
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

    std::cout << "test_node_bitmap_merge() ..." << std::flush;
    test_node_bitmap_merge<T>();
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
