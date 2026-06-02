#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include "../collatz/concepts.hpp"
#include "helpers.hpp"
#include "../collatz/binary_tree_math.hpp"



template<AnySupportedIntegral T>
void test_binary_tree_math_instantiation() {
    start_test(__func__);

    try {
        BinaryTreeMath<T> nope;
        assert(false);
    } catch (std::logic_error& e) {
        assert(std::string(e.what()).find("The BinaryTreeMath class isn't meant to be instanitated.") != std::string::npos);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_math_default_root_value() {
    start_test(__func__);

    // Default should be 1, with an offset of 0.
    assert(BinaryTreeMath<T>::get_default_root_value() == 1);
    assert(BinaryTreeMath<T>::get_root_value() == 1);
    assert(BinaryTreeMath<T>::get_offset() == 0);

    // Assigning to root is limited to 0 or 1.
    try {
        BinaryTreeMath<T>::set_root_value(2);
        assert(false);
    } catch (std::out_of_range& e) {
        assert(std::string(e.what()).find("You cannot set the BinaryTreeMath root value to anything other than 0 or 1.") != std::string::npos);
    }

    // Changing to 0 should work and affect offset but not the default.
    BinaryTreeMath<T>::set_root_value(0);
    assert(BinaryTreeMath<T>::get_default_root_value() == 1);
    assert(BinaryTreeMath<T>::get_root_value() == 0);
    assert(BinaryTreeMath<T>::get_offset() == 1);

    // Changing to 1 should work and affect offset.
    BinaryTreeMath<T>::set_root_value(1);
    assert(BinaryTreeMath<T>::get_default_root_value() == 1);
    assert(BinaryTreeMath<T>::get_root_value() == 1);
    assert(BinaryTreeMath<T>::get_offset() == 0);

    // Reset should set back to one.  Set to 0 to test.
    BinaryTreeMath<T>::set_root_value(0);
    assert(BinaryTreeMath<T>::get_default_root_value() == 1);
    assert(BinaryTreeMath<T>::get_root_value() == 0);
    assert(BinaryTreeMath<T>::get_offset() == 1);
    // Now reset.  Should be 1 again.
    BinaryTreeMath<T>::reset_root_value();
    assert(BinaryTreeMath<T>::get_default_root_value() == 1);
    assert(BinaryTreeMath<T>::get_root_value() == 1);
    assert(BinaryTreeMath<T>::get_offset() == 0);

    // Just in case something was changed...
    BinaryTreeMath<T>::reset_root_value();

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_math_st_node_count_of_level() {
    start_test(__func__);

    // Pick a max level based on type.
    level_t max_levels = (sizeof(T) * 8) - 1;
    if constexpr(GMPIntegral<T>) {
        max_levels = 256;
    }

    // Test them.
    for(level_t level = 1; level < max_levels; level++) {
        T count = T(1) << (level - 1);
        assert(BinaryTreeMath<T>::st_node_count_of_level(level) == count);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_math_st_node_count_of_levels() {
    start_test(__func__);

    // Pick a max level by type.
    level_t max_levels = (sizeof(T) * 8) - 1;
    if constexpr(GMPIntegral<T>) {
        max_levels = 256;
    }

    // Test each level by summing from the single-level math verified above.
    // Minimum level 1 test.
    level_t min_level = 1;
    for(level_t level = min_level; level < max_levels; level++) {
        T count = 0;
        for(level_t sub_level = level; sub_level >= min_level; sub_level--) {
            count += BinaryTreeMath<T>::st_node_count_of_level(sub_level);
        }
        assert(BinaryTreeMath<T>::st_node_count_of_levels(min_level, level) == count);
    }
    // Minimum level 5 test.
    min_level = 5;
    for(level_t level = min_level; level < max_levels; level++) {
        T count = 0;
        for(level_t sub_level = level; sub_level >= min_level; sub_level--) {
            count += BinaryTreeMath<T>::st_node_count_of_level(sub_level);
        }
        assert(BinaryTreeMath<T>::st_node_count_of_levels(min_level, level) == count);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_math_st_node_count_of_tree() {
    start_test(__func__);

    // Pick a max level by type.
    level_t max_levels = (sizeof(T) * 8) - 1;
    if constexpr(GMPIntegral<T>) {
        max_levels = 256;
    }

    // Test each level.
    for(level_t level = 1; level < max_levels; level++) {
        T count = (T(1) << level) - 1;
        assert(BinaryTreeMath<T>::st_node_count_of_tree(level) == count);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_math_st_get_level_by_node_value() {
    start_test(__func__);

    level_t level = 0;
    level = BinaryTreeMath<T>::st_get_level_by_node_value(1);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 2) : assert(level == 1);
    level = BinaryTreeMath<T>::st_get_level_by_node_value(2);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 2) : assert(level == 2);
    level = BinaryTreeMath<T>::st_get_level_by_node_value(3);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 3) : assert(level == 2);
    level = BinaryTreeMath<T>::st_get_level_by_node_value(4);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 3) : assert(level == 3);
    level = BinaryTreeMath<T>::st_get_level_by_node_value(7);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 4) : assert(level == 3);
    level = BinaryTreeMath<T>::st_get_level_by_node_value(14);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 4) : assert(level == 4);
    level = BinaryTreeMath<T>::st_get_level_by_node_value(101);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(level == 7) : assert(level == 7);

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_math_st_max_full_level_at_node_value() {
    start_test(__func__);

    //
    // Max Full Level at Node
    size_t max_level = 0; // Always returns size_t.
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node_value(1);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 1) : assert(max_level == 1);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node_value(4);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 2) : assert(max_level == 2);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node_value(6);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 3) : assert(max_level == 2);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node_value(7);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 3) : assert(max_level == 3);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node_value(26);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 4) : assert(max_level == 4);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node_value(29);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 4) : assert(max_level == 4);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node_value(30);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 5) : assert(max_level == 4);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node_value(31);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 5) : assert(max_level == 5);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node_value(49);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 5) : assert(max_level == 5);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node_value(63);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 6) : assert(max_level == 6);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node_value(125);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 6) : assert(max_level == 6);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node_value(126);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 7) : assert(max_level == 6);
    max_level = BinaryTreeMath<T>::st_max_full_level_at_node_value(127);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(max_level == 7) : assert(max_level == 7);

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_math_st_max_level_of_type() {
    start_test(__func__);

    if constexpr(FixedWidthIntegral<T>) {
        // Get total bits because that's the level limit essentially.
        size_t max_levels = sizeof(T) * 8;
        // Remove one because it's always 2^bits - 1 (for 0).
        max_levels -= 1;
        // Remove one if signed.
        if constexpr(std::numeric_limits<T>::is_signed) {
            max_levels -= 1;
        }
        assert(BinaryTreeMath<T>::st_max_level_of_type() == max_levels);
    } else if constexpr (GMPIntegral<T>) {
        assert(BinaryTreeMath<T>::st_max_level_of_type() == std::numeric_limits<level_t>::max());
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_math_st_level_will_fit() {
    start_test(__func__);

    // Levels are now able to handle bit_width - 1 [-1 if signed].
    // 8-bit
    assert(BinaryTreeMath<uint8_t>::st_level_will_fit(4) == true);
    assert(BinaryTreeMath<uint8_t>::st_level_will_fit(7) == true);
    assert(BinaryTreeMath<uint8_t>::st_level_will_fit(8) == false);
    // 8-bit (signed)
    assert(BinaryTreeMath<int8_t>::st_level_will_fit(4) == true);
    assert(BinaryTreeMath<int8_t>::st_level_will_fit(6) == true);
    assert(BinaryTreeMath<int8_t>::st_level_will_fit(7) == false);
    assert(BinaryTreeMath<int8_t>::st_level_will_fit(8) == false);
    // 16-bit
    assert(BinaryTreeMath<uint16_t>::st_level_will_fit(4) == true);
    assert(BinaryTreeMath<uint16_t>::st_level_will_fit(15) == true);
    assert(BinaryTreeMath<uint16_t>::st_level_will_fit(16) == false);
    // 16-bit (signed)
    assert(BinaryTreeMath<int16_t>::st_level_will_fit(4) == true);
    assert(BinaryTreeMath<int16_t>::st_level_will_fit(14) == true);
    assert(BinaryTreeMath<int16_t>::st_level_will_fit(15) == false);
    assert(BinaryTreeMath<int16_t>::st_level_will_fit(16) == false);
    // 32-bit
    assert(BinaryTreeMath<uint32_t>::st_level_will_fit(10) == true);
    assert(BinaryTreeMath<uint32_t>::st_level_will_fit(31) == true);
    assert(BinaryTreeMath<uint32_t>::st_level_will_fit(32) == false);
    // 32-bit (signed)
    assert(BinaryTreeMath<int32_t>::st_level_will_fit(10) == true);
    assert(BinaryTreeMath<int32_t>::st_level_will_fit(30) == true);
    assert(BinaryTreeMath<int32_t>::st_level_will_fit(31) == false);
    assert(BinaryTreeMath<int32_t>::st_level_will_fit(32) == false);
    // 64-bit
    assert(BinaryTreeMath<uint64_t>::st_level_will_fit(18) == true);
    assert(BinaryTreeMath<uint64_t>::st_level_will_fit(63) == true);
    assert(BinaryTreeMath<uint64_t>::st_level_will_fit(64) == false);
    // 64-bit (signed)
    assert(BinaryTreeMath<int64_t>::st_level_will_fit(18) == true);
    assert(BinaryTreeMath<int64_t>::st_level_will_fit(62) == true);
    assert(BinaryTreeMath<int64_t>::st_level_will_fit(63) == false);
    assert(BinaryTreeMath<int64_t>::st_level_will_fit(64) == false);
    // 128-bit
    assert(BinaryTreeMath<uint128_t>::st_level_will_fit(98) == true);
    assert(BinaryTreeMath<uint128_t>::st_level_will_fit(127) == true);
    assert(BinaryTreeMath<uint128_t>::st_level_will_fit(128) == false);
    // 128-bit (signed)
    assert(BinaryTreeMath<int128_t>::st_level_will_fit(98) == true);
    assert(BinaryTreeMath<int128_t>::st_level_will_fit(126) == true);
    assert(BinaryTreeMath<int128_t>::st_level_will_fit(127) == false);
    assert(BinaryTreeMath<int128_t>::st_level_will_fit(128) == false);
    // MPZ Has No Ceiling
    assert(BinaryTreeMath<mpz_class>::st_level_will_fit(999) == true);

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_math_st_max_node_value_at_level() {
    start_test(__func__);

    // Pick a max level by type.
    level_t max_levels = (sizeof(T) * 8) - 1;
    if constexpr(GMPIntegral<T>) {
        max_levels = 256;
    }

    // Test a few hard-coded levels.
    assert(BinaryTreeMath<T>::st_max_node_value_at_level(1) == T(0) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_max_node_value_at_level(2) == T(2) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_max_node_value_at_level(3) == T(6) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_max_node_value_at_level(4) == T(14) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_max_node_value_at_level(5) == T(30) + BinaryTreeMath<T>::get_root_value());

    // Test each level.
    for(level_t level = 1; level < max_levels; level++) {
        T max_node_value = (T(1) << level) - 1 - BinaryTreeMath<T>::get_offset();
        assert(BinaryTreeMath<T>::st_max_node_value_at_level(level) == max_node_value);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_math_st_first_node_value_of_level() {
    start_test(__func__);

    // Pick a max level by type.
    level_t max_levels = (sizeof(T) * 8) - 1;
    if constexpr(GMPIntegral<T>) {
        max_levels = 256;
    }

    // Test a few hard-coded levels.
    assert(BinaryTreeMath<T>::st_first_node_value_of_level(1) == T(0) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_first_node_value_of_level(2) == T(1) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_first_node_value_of_level(3) == T(3) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_first_node_value_of_level(4) == T(7) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_first_node_value_of_level(5) == T(15) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_first_node_value_of_level(6) == T(31) + BinaryTreeMath<T>::get_root_value());

    // Test each level.
    for(level_t level = 1; level < max_levels; level++) {
        T first_node_value = (T(1) << (level - 1)) - BinaryTreeMath<T>::get_offset();
        assert(BinaryTreeMath<T>::st_first_node_value_of_level(level) == first_node_value);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_math_st_node_value_by_position_and_level() {
    start_test(__func__);

    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(1, 1) == T(0) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(1, 2) == T(1) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(2, 2) == T(2) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(1, 3) == T(3) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(2, 3) == T(5) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(3, 3) == T(4) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(4, 3) == T(6) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(1, 4) == T(7) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(2, 4) == T(11) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(3, 4) == T(9) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(4, 4) == T(13) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(5, 4) == T(8) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(6, 4) == T(12) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(7, 4) == T(10) + BinaryTreeMath<T>::get_root_value());
    assert(BinaryTreeMath<T>::st_node_value_by_position_and_level(8, 4) == T(14) + BinaryTreeMath<T>::get_root_value());
    //
    // Deprecated Form
    // assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(1, 1) == 0 + BinaryTreeMath<T>::get_root_value());
    // assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(1, 2) == 1 + BinaryTreeMath<T>::get_root_value());
    // assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(2, 2) == 2 + BinaryTreeMath<T>::get_root_value());
    // assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(1, 3) == 3 + BinaryTreeMath<T>::get_root_value());
    // assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(2, 3) == 5 + BinaryTreeMath<T>::get_root_value());
    // assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(3, 3) == 4 + BinaryTreeMath<T>::get_root_value());
    // assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(4, 3) == 6 + BinaryTreeMath<T>::get_root_value());
    // assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(1, 4) == 7 + BinaryTreeMath<T>::get_root_value());
    // assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(2, 4) == 11 + BinaryTreeMath<T>::get_root_value());
    // assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(3, 4) == 9 + BinaryTreeMath<T>::get_root_value());
    // assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(4, 4) == 13 + BinaryTreeMath<T>::get_root_value());
    // assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(5, 4) == 8 + BinaryTreeMath<T>::get_root_value());
    // assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(6, 4) == 12 + BinaryTreeMath<T>::get_root_value());
    // assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(7, 4) == 10 + BinaryTreeMath<T>::get_root_value());
    // assert(BinaryTreeMath<T>::st_node_value_by_position_and_level__deprecated(8, 4) == 14 + BinaryTreeMath<T>::get_root_value());
    //
    // Out-Param Version
    T out;
    BinaryTreeMath<T>::st_node_value_by_position_and_level(1, 1, out);
    assert(out == T(0) + BinaryTreeMath<T>::get_root_value());
    BinaryTreeMath<T>::st_node_value_by_position_and_level(1, 2, out);
    assert(out == T(1) + BinaryTreeMath<T>::get_root_value());
    BinaryTreeMath<T>::st_node_value_by_position_and_level(2, 2, out);
    assert(out == T(2) + BinaryTreeMath<T>::get_root_value());
    BinaryTreeMath<T>::st_node_value_by_position_and_level(1, 3, out);
    assert(out == T(3) + BinaryTreeMath<T>::get_root_value());
    BinaryTreeMath<T>::st_node_value_by_position_and_level(2, 3, out);
    assert(out == T(5) + BinaryTreeMath<T>::get_root_value());
    BinaryTreeMath<T>::st_node_value_by_position_and_level(3, 3, out);
    assert(out == T(4) + BinaryTreeMath<T>::get_root_value());
    BinaryTreeMath<T>::st_node_value_by_position_and_level(4, 3, out);
    assert(out == T(6) + BinaryTreeMath<T>::get_root_value());
    BinaryTreeMath<T>::st_node_value_by_position_and_level(1, 4, out);
    assert(out == T(7) + BinaryTreeMath<T>::get_root_value());
    BinaryTreeMath<T>::st_node_value_by_position_and_level(2, 4, out);
    assert(out == T(11) + BinaryTreeMath<T>::get_root_value());
    BinaryTreeMath<T>::st_node_value_by_position_and_level(3, 4, out);
    assert(out == T(9) + BinaryTreeMath<T>::get_root_value());
    BinaryTreeMath<T>::st_node_value_by_position_and_level(4, 4, out);
    assert(out == T(13) + BinaryTreeMath<T>::get_root_value());
    BinaryTreeMath<T>::st_node_value_by_position_and_level(5, 4, out);
    assert(out == T(8) + BinaryTreeMath<T>::get_root_value());
    BinaryTreeMath<T>::st_node_value_by_position_and_level(6, 4, out);
    assert(out == T(12) + BinaryTreeMath<T>::get_root_value());
    BinaryTreeMath<T>::st_node_value_by_position_and_level(7, 4, out);
    assert(out == T(10) + BinaryTreeMath<T>::get_root_value());
    BinaryTreeMath<T>::st_node_value_by_position_and_level(8, 4, out);
    assert(out == T(14) + BinaryTreeMath<T>::get_root_value());

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_math_st_node_position() {
    start_test(__func__);

    T position = 0;
    position = BinaryTreeMath<T>::st_node_position(1);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(position == 1) : assert(position == 1);
    position = BinaryTreeMath<T>::st_node_position(2);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(position == 2) : assert(position == 1);
    position = BinaryTreeMath<T>::st_node_position(3);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(position == 1) : assert(position == 2);
    position = BinaryTreeMath<T>::st_node_position(4);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(position == 3) : assert(position == 1);
    position = BinaryTreeMath<T>::st_node_position(5);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(position == 2) : assert(position == 3);
    position = BinaryTreeMath<T>::st_node_position(6);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(position == 4) : assert(position == 2);
    position = BinaryTreeMath<T>::st_node_position(7);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(position == 1) : assert(position == 4);
    position = BinaryTreeMath<T>::st_node_position(46);
    BinaryTreeMath<T>::get_root_value() == 0 ? assert(position == 31) : assert(position == 15);

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_math_st_fg_chain_length() {
    start_test(__func__);

    level_t max_levels = 256;
    for (level_t level = 1; level <= max_levels; level++) {
        seq_size_t fg_chain_length = level - 1;
        assert(BinaryTreeMath<T>::st_fg_chain_length(level) == fg_chain_length);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_math_st_floor_log2() {
    start_test(__func__);

    // Manual ones at edges.
    assert(BinaryTreeMath<T>::st_floor_log2(T(1)) == 0);
    assert(BinaryTreeMath<T>::st_floor_log2(T(2)) == 1);
    assert(BinaryTreeMath<T>::st_floor_log2(T(3)) == 1);
    assert(BinaryTreeMath<T>::st_floor_log2(T(4)) == 2);

    T max_value = 1000000;
    for (T value = 0; value <= max_value; value++) {
        size_t floor_log2;
        if (value == 0) {
            floor_log2 = 0;
        } else {
            if constexpr(FixedWidthIntegral<T>) {
                floor_log2 = std::floor(std::log2(static_cast<uint64_t>(value)));
            } else if constexpr(GMPIntegral<T>) {
                floor_log2 = std::floor(std::log2((value.get_ui())));
            }
        }
        assert(BinaryTreeMath<T>::st_floor_log2(value) == floor_log2);
    }

    end_test();
}



template<AnySupportedIntegral T>
void test_binary_tree_math_st_scaling_factor() {
    start_test(__func__);

    // Pick a max level by type.
    level_t max_levels = (sizeof(T) * 8) - 1;
    if constexpr(GMPIntegral<T>) {
        max_levels = 256;
    }

    // Test them.  Level 1 should fail.
    try {
        T junk = BinaryTreeMath<T>::st_scaling_factor(1);
        assert(false);
        junk++;
    } catch (std::out_of_range& e) {
        assert(std::string(e.what()).find("Cannot request a scaling factor for levels below 2.") != std::string::npos);
    }
    for (level_t level = 2; level <= max_levels; level++) {
        assert(BinaryTreeMath<T>::st_scaling_factor(level) == (T(1) << (level - 2)));
    }

    end_test();
}



template<AnySupportedIntegral T>
void run_all() {
    announce_run_all<T>();
    test_binary_tree_math_instantiation<T>();
    test_binary_tree_math_default_root_value<T>();
    test_binary_tree_math_st_node_count_of_level<T>();
    test_binary_tree_math_st_node_count_of_levels<T>();
    test_binary_tree_math_st_node_count_of_tree<T>();
    test_binary_tree_math_st_get_level_by_node_value<T>();
    test_binary_tree_math_st_max_full_level_at_node_value<T>();
    test_binary_tree_math_st_max_level_of_type<T>();
    test_binary_tree_math_st_level_will_fit<T>();
    test_binary_tree_math_st_max_node_value_at_level<T>();
    test_binary_tree_math_st_first_node_value_of_level<T>();
    test_binary_tree_math_st_node_value_by_position_and_level<T>();
    test_binary_tree_math_st_node_position<T>();
    test_binary_tree_math_st_fg_chain_length<T>();
    test_binary_tree_math_st_floor_log2<T>();
    test_binary_tree_math_st_scaling_factor<T>();
}



int main() {
    std::string name = "BinaryTreeMath";
    preamble(name);
    run_all<uint64_t>();
    run_all<uint128_t>();
    run_all<mpz_class>();
    done(name);

    return 0;
}
