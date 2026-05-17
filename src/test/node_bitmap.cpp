#include <atomic>
#include <cassert>
#include <cstdint>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"
#include "../collatz/node_bitmap.hpp"
#include "../collatz/node_bitmap_traits.hpp"




template<AnySupportedIntegral T>
void test_node_bitmap_basics() {
    start_test(__func__);

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

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_cross_prefix_boundary() {
    start_test(__func__);

    // Test into a larger prefix.
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

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_clear() {
    start_test(__func__);

    NodeBitmap<T> bitmap;
    assert(bitmap.contains(1) == false);
    bitmap.add(1);
    assert(bitmap.contains(1) == true);
    bitmap.clear();
    assert(bitmap.contains(1) == false);

    using Traits = BitmapKeyTraits<T>;
    T value_larger_than_suffix = T(1) << Traits::SUFFIX_BITS;
    assert(bitmap.contains(value_larger_than_suffix) == false);
    bitmap.add(value_larger_than_suffix);
    assert(bitmap.contains(value_larger_than_suffix) == true);
    bitmap.clear();
    assert(bitmap.contains(value_larger_than_suffix) == false);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_get_impl() {
    start_test(__func__);

    NodeBitmap<T> bitmap;
    const FlatHashBitmapImpl<T>& impl = bitmap.get_impl();
    using ImplType = decltype(impl);
    assert((std::is_same_v<std::decay_t<ImplType>, FlatHashBitmapImpl<T>>));

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_add() {
    start_test(__func__);

    NodeBitmap<T> bitmap;

    // Test within a single prefix.
    bitmap.clear();
    assert(bitmap.cardinality() == 0);
    for (T i = 0; i < 1000000; i++) {
        assert(bitmap.contains(i) == false);
        bitmap.add(i);
        assert(bitmap.contains(i) == true);
    }

    // Test across two prefixes.
    bitmap.clear();
    assert(bitmap.cardinality() == 0);
    T start = BitmapKeyTraits<T>::SUFFIX_MAX - 100;
    T end = start + 1000000;
    for (T i = start; i < end; i++) {
        assert(bitmap.contains(i) == false);
        bitmap.add(i);
        assert(bitmap.contains(i) == true);
    }

    // Test across many prefixes.
    bitmap.clear();
    assert(bitmap.cardinality() == 0);
    T suffix_value = BitmapKeyTraits<T>::SUFFIX_MAX;
    T max_prefixes = 100000;
    for (T i = 0; i < max_prefixes; i++) {
        T value = (T(i) << BitmapKeyTraits<T>::SUFFIX_BITS) + suffix_value;
        assert(bitmap.contains(value) == false);
        bitmap.add(value);
        assert(bitmap.contains(value) == true);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_add_range() {
    start_test(__func__);

    NodeBitmap<T> bitmap;
    T start = 0;
    T end = 0;

    // Within a single prefix.
    bitmap.clear();
    assert(bitmap.cardinality() == 0);
    start = 0;
    end = 1234;
    bitmap.add_range(start, end);
    for (T value = start; value < end; value++) {
        assert(bitmap.contains(value) == true);
    }
    assert(bitmap.contains(end) == false);

    // Cross prefixes.
    bitmap.clear();
    assert(bitmap.cardinality() == 0);
    start = (T(1) << BitmapKeyTraits<T>::SUFFIX_BITS) - 1;
    end = start + 1234;
    bitmap.add_range(start, end);
    for (T value = start; value < end; value++) {
        assert(bitmap.contains(value) == true);
    }
    assert(bitmap.contains(end) == false);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_add_range_closed() {
    start_test(__func__);

    NodeBitmap<T> bitmap;
    T start = 0;
    T end = 0;

    // Within a single prefix.
    bitmap.clear();
    assert(bitmap.cardinality() == 0);
    start = 0;
    end = 1234;
    bitmap.add_range_closed(start, end);
    for (T value = start; value < end; value++) {
        assert(bitmap.contains(value) == true);
    }
    assert(bitmap.contains(end) == true);
    assert(bitmap.contains(end + 1) == false);

    // Cross prefixes.
    bitmap.clear();
    assert(bitmap.cardinality() == 0);
    start = (T(1) << BitmapKeyTraits<T>::SUFFIX_BITS) - 1;
    end = start + 1234;
    bitmap.add_range_closed(start, end);
    for (T value = start; value < end; value++) {
        assert(bitmap.contains(value) == true);
    }
    assert(bitmap.contains(end) == true);
    assert(bitmap.contains(end + 1) == false);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_contains() {
    start_test(__func__);

    NodeBitmap<T> bitmap;
    bitmap.clear();
    assert(bitmap.cardinality() == 0);
    T start = 0;
    T end = start + 1000000;
    bitmap.add_range_closed(start, end);
    for (T value = start; value <= end; value++) {
        assert(bitmap.contains(value) == true);
    }
    for (T value = end + 1; value <= end + 1000000; value++) {
        assert(bitmap.contains(value) == false);
    }


    // Cross a prefix boundary.
    bitmap.clear();
    assert(bitmap.cardinality() == 0);
    start = BitmapKeyTraits<T>::SUFFIX_MAX;
    end = start + 1000000;
    bitmap.add_range_closed(start, end);
    for (T value = start; value <= end; value++) {
        assert(bitmap.contains(value) == true);
    }
    for (T value = end + 1; value <= end + 1000000; value++) {
        assert(bitmap.contains(value) == false);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_remove() {
    start_test(__func__);

    NodeBitmap<T> bitmap;
    bitmap.clear();
    assert(bitmap.cardinality() == 0);
    T start = 0;
    T end = start + 1000000;
    bitmap.add_range_closed(start, end);
    for (T value = start; value <= end; value++) {
        assert(bitmap.contains(value) == true);
        bitmap.remove(value);
        assert(bitmap.contains(value) == false);
    }
    assert(bitmap.cardinality() == 0);

    // Cross a prefix boundary.
    bitmap.clear();
    assert(bitmap.cardinality() == 0);
    start = BitmapKeyTraits<T>::SUFFIX_MAX;
    end = start + 1000000;
    bitmap.add_range_closed(start, end);
    for (T value = start; value <= end; value++) {
        assert(bitmap.contains(value) == true);
        bitmap.remove(value);
        assert(bitmap.contains(value) == false);
    }
    assert(bitmap.cardinality() == 0);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_cardinality() {
    start_test(__func__);

    NodeBitmap<T> bitmap;

    // Single prefix.
    T start = 0;
    for (T end = start; end < (start + 1000000); end++) {
        bitmap.clear();
        assert(bitmap.cardinality() == 0);
        bitmap.add_range(start, end);
        assert(bitmap.cardinality() == (end - start));
    }

    // Across prefixes.
    start = BitmapKeyTraits<T>::SUFFIX_MAX - 42;
    for (T end = start; end < (start + 1000000); end++) {
        bitmap.clear();
        assert(bitmap.cardinality() == 0);
        bitmap.add_range(start, end);
        assert(bitmap.cardinality() == (end - start));
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_merge() {
    start_test(__func__);

    NodeBitmap<T> bitmap_src;
    NodeBitmap<T> bitmap_dst;

    // Basic merge
    bitmap_src.add(1);
    bitmap_src.add(2);
    bitmap_src.add(3);
    assert(bitmap_src.cardinality() == 3);
    assert(bitmap_dst.cardinality() == 0);
    bitmap_dst.merge(bitmap_src);
    assert(bitmap_dst.cardinality() == 3);
    assert(bitmap_dst.contains(1) == true);
    assert(bitmap_dst.contains(2) == true);
    assert(bitmap_dst.contains(3) == true);
    assert(bitmap_dst.contains(4) == false);

    // Now redo it with pre-existing values.
    bitmap_dst.clear();
    assert(bitmap_dst.cardinality() == 0);
    bitmap_dst.add(42);
    assert(bitmap_dst.contains(42) == true);
    bitmap_dst.merge(bitmap_src);
    assert(bitmap_dst.cardinality() == 4);
    assert(bitmap_dst.contains(1) == true);
    assert(bitmap_dst.contains(2) == true);
    assert(bitmap_dst.contains(3) == true);
    assert(bitmap_dst.contains(4) == false);
    assert(bitmap_dst.contains(42) == true);

    // Now merge values in multiple prefixes.
    T big_src_value = T(123) << BitmapKeyTraits<T>::SUFFIX_BITS;
    T big_dst_value = big_src_value + 1;
    bitmap_dst.clear();
    assert(bitmap_dst.cardinality() == 0);
    bitmap_src.add(big_src_value);  // Put one in src.
    bitmap_dst.add(big_dst_value);  // Put another in dst.
    assert(bitmap_dst.contains(big_dst_value) == true);
    bitmap_dst.merge(bitmap_src);
    assert(bitmap_dst.cardinality() == 5);
    assert(bitmap_dst.contains(1) == true);
    assert(bitmap_dst.contains(2) == true);
    assert(bitmap_dst.contains(3) == true);
    assert(bitmap_dst.contains(4) == false);
    assert(bitmap_dst.contains(42) == false);
    assert(bitmap_dst.contains(big_src_value) == true);
    assert(bitmap_dst.contains(big_dst_value) == true);

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
    assert(bitmap_dst.contains(1) == true);
    assert(bitmap_dst.contains(2) == true);
    assert(bitmap_dst.contains(3) == true);
    assert(bitmap_dst.contains(4) == false);

    // Now redo it with pre-existing values.
    bitmap_dst.clear();
    assert(bitmap_dst.cardinality() == 0);
    bitmap_dst.add(42);
    assert(bitmap_dst.contains(42));
    bitmap_dst |= bitmap_src;
    assert(bitmap_dst.cardinality() == 4);
    assert(bitmap_dst.contains(1) == true);
    assert(bitmap_dst.contains(2) == true);
    assert(bitmap_dst.contains(3) == true);
    assert(bitmap_dst.contains(4) == false);
    assert(bitmap_dst.contains(42) == true);

    // Now merge values in multiple prefixes.
    big_src_value = T(123) << BitmapKeyTraits<T>::SUFFIX_BITS;
    big_dst_value = big_src_value + 1;
    bitmap_dst.clear();
    assert(bitmap_dst.cardinality() == 0);
    bitmap_src.add(big_src_value);  // Put one in src.
    bitmap_dst.add(big_dst_value);  // Put another in dst.
    assert(bitmap_dst.contains(big_dst_value) == true);
    bitmap_dst |= bitmap_src;
    assert(bitmap_dst.cardinality() == 5);
    assert(bitmap_dst.contains(1) == true);
    assert(bitmap_dst.contains(2) == true);
    assert(bitmap_dst.contains(3) == true);
    assert(bitmap_dst.contains(4) == false);
    assert(bitmap_dst.contains(42) == false);
    assert(bitmap_dst.contains(big_src_value) == true);
    assert(bitmap_dst.contains(big_dst_value) == true);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_clone() {
    start_test(__func__);

    NodeBitmap<T> bitmap;
    NodeBitmap<T> bitmap_dst;

    T cross_prefix_value = T(BitmapKeyTraits<T>::SUFFIX_MAX) + 1234;
    bitmap.add(1);
    bitmap.add(2);
    bitmap.add(3);
    bitmap.add(cross_prefix_value);

    assert(bitmap_dst.contains(1) == false);
    assert(bitmap_dst.contains(2) == false);
    assert(bitmap_dst.contains(3) == false);
    assert(bitmap_dst.contains(cross_prefix_value) == false);
    bitmap_dst.clone(bitmap);
    assert(bitmap_dst.contains(1) == true);
    assert(bitmap_dst.contains(2) == true);
    assert(bitmap_dst.contains(3) == true);
    assert(bitmap_dst.contains(cross_prefix_value) == true);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_optimize() {
    start_test(__func__);

    NodeBitmap<T> bitmap;
    bitmap.add_range(BitmapKeyTraits<T>::SUFFIX_MAX, BitmapKeyTraits<T>::SUFFIX_MAX * 2);

    // The remove() method cleans up on its own... just make sure invocation works for now.
    bitmap.optimize();

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_shrink_to_fit() {
    start_test(__func__);

    NodeBitmap<T> bitmap;
    bitmap.add_range(BitmapKeyTraits<T>::SUFFIX_MAX, BitmapKeyTraits<T>::SUFFIX_MAX * 2);

    // The remove() method cleans up on its own... just make sure invocation works for now.
    bitmap.shrink_to_fit();

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_deep_size() {
    start_test(__func__);

    NodeBitmap<T> bitmap;
    assert(bitmap.deep_size() > 1);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_cloneable() {
    start_test(__func__);

    NodeBitmap<T> bitmap;
    bitmap.add_range(BitmapKeyTraits<T>::SUFFIX_MAX, BitmapKeyTraits<T>::SUFFIX_MAX * 2);
    assert(cloneable(bitmap));

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_equality() {
    start_test(__func__);

    NodeBitmap<T> first;
    NodeBitmap<T> second;
    NodeBitmap<T> different;
    first.add_range(BitmapKeyTraits<T>::SUFFIX_MAX, BitmapKeyTraits<T>::SUFFIX_MAX * 2);
    second.add_range(BitmapKeyTraits<T>::SUFFIX_MAX, BitmapKeyTraits<T>::SUFFIX_MAX * 2);
    different.add_range(1, 10);
    assert(equality(first, second, different));

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_for_each_value() {
    start_test(__func__);

    NodeBitmap<T> bitmap;
    size_t count = 1000000;
    T start = BitmapKeyTraits<T>::SUFFIX_MAX;
    T end = start + count;
    bitmap.add_range(start, end);

    // Serial
    size_t tally = 0;
    bitmap.for_each_value(ForEachPolicy::SERIAL, [&](const T& value) {
        assert(bitmap.contains(value));
        tally++;
        return false;
    });
    assert(tally == count);

    // Parallel
    std::atomic<size_t> atomic_tally = 0;
    bitmap.for_each_value(ForEachPolicy::PARALLEL, [&](const T& value) {
        assert(bitmap.contains(value));
        atomic_tally.fetch_add(1, std::memory_order_relaxed);
        return false;
    });
    assert(atomic_tally.load() == count);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_for_each_transformer() {
    start_test(__func__);

    NodeBitmap<T> bitmap;
    size_t count = 1000000;
    T start = BitmapKeyTraits<T>::SUFFIX_MAX;
    T end = start + count;
    bitmap.add_range(start, end);

    // Serial
    std::vector<NodeBitmap<T>> callback_storage;
    callback_storage.resize(1);
    bitmap.for_each_transformer(ForEachPolicy::SERIAL, callback_storage, [&](const T& value, NodeBitmap<T>& tls) {
        assert(bitmap.contains(value));
        tls.add(value);
        return false;
    });
    assert(bitmap.cardinality() == callback_storage[0].cardinality());
    callback_storage[0].for_each_value(ForEachPolicy::SERIAL, [&](const T& value) {
        assert(bitmap.contains(value));
        return false;
    });

    // Parallel
    bitmap.for_each_transformer(ForEachPolicy::PARALLEL, callback_storage, [&](const T& value, NodeBitmap<T>& tls) {
        assert(bitmap.contains(value));
        tls.add(value);
        return false;
    });
    // Merge TLS into a single map.
    NodeBitmap<T> transformed_map;
    for(const NodeBitmap<T>& storage : callback_storage) {
        transformed_map |= storage;
    }
    assert(bitmap.cardinality() == transformed_map.cardinality());
    transformed_map.for_each_value(ForEachPolicy::SERIAL, [&](const T& value) {
        assert(bitmap.contains(value));
        return false;
    });

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_node_bitmap_basics<T>();
    test_node_bitmap_cross_prefix_boundary<T>();
    test_node_bitmap_clear<T>();
    test_node_bitmap_get_impl<T>();
    test_node_bitmap_add<T>();
    test_node_bitmap_add_range<T>();
    test_node_bitmap_add_range_closed<T>();
    test_node_bitmap_contains<T>();
    test_node_bitmap_remove<T>();
    test_node_bitmap_cardinality<T>();
    test_node_bitmap_merge<T>();
    test_node_bitmap_clone<T>();
    test_node_bitmap_optimize<T>();
    test_node_bitmap_shrink_to_fit<T>();
    test_node_bitmap_deep_size<T>();
    test_node_bitmap_cloneable<T>();
    test_node_bitmap_equality<T>();
    test_node_bitmap_for_each_value<T>();
    test_node_bitmap_for_each_transformer<T>();
}



int main() {
    std::string name = "NodeBitmap";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
