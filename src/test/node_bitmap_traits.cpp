#include <cassert>
#include <cstdint>
#include <limits>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"
#include "../collatz/node_bitmap_traits.hpp"



template<AnySupportedIntegral T>
void test_node_bitmap_traits_transformer_policy() {
    start_test(__func__);

    assert(static_cast<int>(BitmapTransformerPolicy::SERIAL) == 1);
    assert(static_cast<int>(BitmapTransformerPolicy::PARALLEL) == 2);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_traits_suffix_t() {
    start_test(__func__);

    // Suffix is a uint32_t in Roaring.  Ensure that.
    using Traits = BitmapKeyTraits<T>;
    using suffix_t = Traits::suffix_t;
    assert((std::is_same_v<suffix_t, uint32_t>));
    assert(Traits::SUFFIX_BYTES == sizeof(uint32_t));
    assert(Traits::SUFFIX_BITS == sizeof(uint32_t) * 8);
    assert(Traits::SUFFIX_MASK == std::numeric_limits<uint32_t>::max());
    assert(Traits::SUFFIX_MAX == std::numeric_limits<uint32_t>::max());

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_traits_prefix_t() {
    start_test(__func__);

    // Prefix varies by type.
    using Traits = BitmapKeyTraits<T>;
    using prefix_t = Traits::prefix_t;
    if constexpr(FixedWidthIntegral<T>) {
        if constexpr(sizeof(T) <= 8) {
            assert((std::is_same_v<prefix_t, uint32_t>));
        } else {
            assert((std::is_same_v<prefix_t, uint128_t>));
        }
    } else if constexpr (GMPIntegral<T>) {
        assert((std::is_same_v<prefix_t, mpz_class>));
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_traits_roaring_types() {
    start_test(__func__);

    using Traits = BitmapKeyTraits<T>;
    using roaring_key_t = Traits::roaring_key_t;
    using roaring_value_t = Traits::roaring_value_t;
    using roaring_typecode_t = Traits::roaring_typecode_t;
    using roaring_word_t = Traits::roaring_word_t;
    assert((std::is_same_v<roaring_key_t, uint16_t>));
    assert((std::is_same_v<roaring_value_t, uint16_t>));
    assert((std::is_same_v<roaring_typecode_t, uint8_t>));
    assert((std::is_same_v<roaring_word_t, uint64_t>));
    assert(sizeof(roaring_key_t) * 8 == Traits::ROARING_KEY_BITS);
    assert(sizeof(roaring_value_t) * 8 == Traits::ROARING_VALUE_BITS);
    assert(sizeof(roaring_word_t) * 8 == Traits::ROARING_WORD_BITS);

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_traits_get_prefix() {
    start_test(__func__);

    using Traits = BitmapKeyTraits<T>;
    using prefix_t = Traits::prefix_t;
    prefix_t out = 0;

    // Incremental prefixes.
    for (T i = 0; i < 100000; i++) {
        T value = T(1) + (i << Traits::SUFFIX_BITS);
        Traits::get_prefix(value, out);
        assert(Traits::get_prefix(value) == prefix_t(i));
        assert(out == prefix_t(i));
    }

    // Huge prefixes.
    if constexpr(FixedWidthIntegral<T>) {
        // Drag T back by SUFFIX_BITS to avoid overflow when shifting 'i'.
        T T_max = T(1) << ((sizeof(T) * 8) - Traits::SUFFIX_BITS);
        T lower_limit = T_max - 100000;
        for (T i = lower_limit; i < T_max; i++) {
            T value = T(1) + (i << Traits::SUFFIX_BITS);
            Traits::get_prefix(value, out);
            assert(Traits::get_prefix(value) == prefix_t(i));
            assert(out == prefix_t(i));
        }
    } else if constexpr(GMPIntegral<T>) {
        // Prove GMP can match 128-bit.
        T T_max = T(1) << ((sizeof(uint128_t) * 8) - Traits::SUFFIX_BITS);
        T lower_limit = T_max - 100000;
        for (T i = lower_limit; i < T_max; i++) {
            T value = T(1) + (i << Traits::SUFFIX_BITS);
            Traits::get_prefix(value, out);
            assert(Traits::get_prefix(value) == prefix_t(i));
            assert(out == prefix_t(i));
        }
        // Now prove it can go beyond it.
        T_max = T(1) << 256;
        lower_limit = T_max - 100000;
        for (T i = lower_limit; i < T_max; i++) {
            T value = T(1) + (i << Traits::SUFFIX_BITS);
            Traits::get_prefix(value, out);
            assert(Traits::get_prefix(value) == prefix_t(i));
            assert(out == prefix_t(i));
        }
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_node_bitmap_traits_get_suffix() {
    start_test(__func__);

    using Traits = BitmapKeyTraits<T>;
    using suffix_t = Traits::suffix_t;

    for(suffix_t i = 0; i < 100000; i++) {
        T value = (T(1) << Traits::SUFFIX_BITS) + i;
        assert(Traits::get_suffix(value) == suffix_t(i));
    }

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();

    test_node_bitmap_traits_transformer_policy<T>();
    test_node_bitmap_traits_suffix_t<T>();
    test_node_bitmap_traits_prefix_t<T>();
    test_node_bitmap_traits_roaring_types<T>();
    test_node_bitmap_traits_get_prefix<T>();
    test_node_bitmap_traits_get_suffix<T>();
}



int main() {
    std::string name = "NodeBitmapTraits";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
