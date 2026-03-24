#pragma once
#include <gmp.h>
#include <stdexcept>
#include <cmath>
#include "concepts.hpp"
#include "collatz_constants.hpp"
#include "exponents.hpp"
#include "bitreverse_helpers.hpp"


//
// The BinaryTree class could maintain all of this, but we might want to seprate the math from the tree itself, even though the
// tree will intrinsically build (via add_level()) according to this math.
//
// A Note About Indexes and Positions
//     - Levels are the vertical coordinate of the tree and they are labeled with the counting numbers: 1, 2, 3...
//     - Positions are the horizontal coordinate of the tree and they are labeled with the counting numbers: 1, 2, 3...
// This means levels and positions are quantities by default, not zero-based indexes.  We use unordered maps internally with level
// as the key.  This means you can safely iterate keys or indexes *if* you start with i=1, not i=0.
//
// A Note About 0- and 1-Based Roots
// The research for this project initially built the tree with a 0-based root node.  This was later changed to be 1-based, but we
// kept the 0-based logic available by extracting the math into this BinaryTreeMath class and tracking some offsets and such.  You
// can expect 1-based trees to work perfect, and we've given a best effort for 0-based.
//





//
// LevelInterval
//
// Implicit trees leverage NodeCoordinates by converting them to ranges that represent covered or uncovered nodes.
//
template<AnySupportedIntegral T>
struct Interval {
    // Disallow sizes smaller than 16 bits to avoid a GCC bug when used in vectors.
    using storage_t = std::conditional_t<(sizeof(T) < sizeof(uint16_t)), uint16_t, T>;
    storage_t start; // Inclusive
    storage_t end;   // Inclusive

    storage_t size() const { return end >= start ? (end - start + 1) : storage_t(0); }
};



//
// BinaryTreeMath
//
template<AnySupportedIntegral T>
class BinaryTreeMath {
    private:
    static inline const mpz_class _MPZ_TWO = 2;
    static inline const size_t _ROOT_VALUE_DEFAULT = 1;
    static inline size_t _root_value = _ROOT_VALUE_DEFAULT;
    static inline size_t _offset = 1 - _root_value;

    public:
    // Don't instanitate this.
    BinaryTreeMath() {
        throw std::logic_error("The BinaryTreeMath class isn't meant to be instanitated.");
    }



    //
    // Getter and Setter for Root Value
    //
    static size_t get_default_root_value() { return _ROOT_VALUE_DEFAULT; }
    static size_t get_root_value() { return _root_value; }
    static void reset_root_value() { _root_value = _ROOT_VALUE_DEFAULT; }
    static size_t get_offset() { return _offset; }
    static void set_root_value(size_t value) {
        if (value != 0 && value != 1) {
            throw std::out_of_range("You cannot set the BinaryTreeMath root value to anything other than 0 or 1.");
        }
        _root_value = value;
        _offset = 1 - _root_value;
    }



    //
    // Node Count of Tree
    // The number of nodes in a tree of level L depth.
    //
    // Formula: 2^(level) - 1
    static inline T st_node_count_of_tree(size_t levels) {
        static thread_local T count;
        if constexpr(BuiltinIntegral<T>) {
            count = (T(1) << levels) - 1;
        } else if constexpr(GMPIntegral<T>) {
            mpz_pow_ui(count.get_mpz_t(), CollatzConstants::MPZ_TWO.get_mpz_t(), levels);
            mpz_sub_ui(count.get_mpz_t(), count.get_mpz_t(), 1);
        } else {
            throw std::logic_error("Unknown type.");
        }
        return count;
    }



    //
    // Node Count of Level
    // The number of nodes in a tree's level L.
    //
    // Formula: 2^(level - 1)
    static inline T st_node_count_of_level(size_t level) {
        static thread_local T count;
        if constexpr(BuiltinIntegral<T>) {
            count = T(1) << (level - 1);
        } else if constexpr(GMPIntegral<T>) {
            mpz_pow_ui(count.get_mpz_t(), CollatzConstants::MPZ_TWO.get_mpz_t(), (level - 1));
        } else {
            throw std::logic_error("Unknown type.");
        }
        return count;
    }



    //
    // Node Count of Levels
    // The number of nodes in a tree between min_level and max_level, inclusive.
    //
    // Formula: (2^(max_level) - 1) - (2^(min_level-1) - 1)
    static inline T st_node_count_of_levels(size_t min_level, size_t max_level) {
        static thread_local T full_tree_count;
        static thread_local T sub_tree_count;
        static thread_local T final_count;
        if constexpr(BuiltinIntegral<T>) {
            final_count = (T(1) << max_level) - (T(1) << (min_level - 1));
        } else if constexpr(GMPIntegral<T>) {
            mpz_pow_ui(full_tree_count.get_mpz_t(), CollatzConstants::MPZ_TWO.get_mpz_t(), max_level);
            mpz_pow_ui(sub_tree_count.get_mpz_t(), CollatzConstants::MPZ_TWO.get_mpz_t(), min_level - 1);
            mpz_sub(final_count.get_mpz_t(), full_tree_count.get_mpz_t(), sub_tree_count.get_mpz_t());
        } else {
            throw std::logic_error("Unknown type.");
        }
        return final_count;
    }



    //
    // Bit Reversal
    // The nature of the binary tree structure means certain liberties can be taken when calculating positions or node values
    // within the tree, largely due to the constant power-of-two increase.  To leverage this, we need to take bits from a node
    // value or position and reverse them, but only a certain number of them on the LSB (least significant bit) side.  There are a
    // lot of ways to do this, including a basic loop, which I used originally and have removed (diff 3.2.0).  We will use compiler
    // intrisics and more black-box-ish work to speed this up.
    //
    // First we make a general full-reverse method.
    static inline T st_bit_reverse_full(const T& x) {
        if constexpr(NativeIntegral<T>) {
            // Native types have a direct builtin.  Use it.
            using U = std::make_unsigned_t<T>;
            U ux = static_cast<U>(x);
            if constexpr (sizeof(T) == 1) {
                return static_cast<T>(bitreverse8(ux));
            } else if constexpr (sizeof(T) == 2) {
                return static_cast<T>(bitreverse16(ux));
            } else if constexpr (sizeof(T) == 4) {
                return static_cast<T>(bitreverse32(ux));
            } else if constexpr (sizeof(T) == 8) {
                return static_cast<T>(bitreverse64(ux));
            }
        } else if constexpr(ExtendedIntegral<T>) {
            // Extended 128-bit integrals need a little juggling.
            using U = make_unsigned_custom_t<T>;
            U ux = static_cast<U>(x);

            uint64_t low  = (uint64_t)ux;
            uint64_t high = (uint64_t)(ux >> 64);
            uint64_t rev_low  = bitreverse64(high);
            uint64_t rev_high = bitreverse64(low);
            return static_cast<T>(( (U)rev_high << 64 ) | rev_low);
        } else if constexpr(GMPIntegral<T>) {
            // GMP is arbitrary precision, so we need to get the current size/width and juggle limbs.
            throw std::logic_error("Cannot fully reverse mpz_class");
        } else {
            static_assert(false, "Must use a supported type T");
        }
    }
    //
    //
    // Now the reversal of the lowest bits.
    // Formula: reverse(bits)[0:size]
    static inline T st_reverse_low_bits(const T& value, size_t bits) {
        // Test for zero.
        T result = 0;
        if (bits == 0) { return result; }

        // Reverse and shift by the correct amount.
        if constexpr(BuiltinIntegral<T>) {
            result = st_bit_reverse_full(value);
            constexpr size_t WIDTH = sizeof(T) * 8;
            result >>= (WIDTH - bits);
        } else if constexpr(GMPIntegral<T>) {
            for (size_t bit = 0; bit < bits; bit++) {
                if (mpz_tstbit(value.get_mpz_t(), bit)) {
                    mpz_setbit(result.get_mpz_t(), bits - 1 - bit);
                }
            }
        }

        // Return.
        return result;
    }
    //
    // Alternate version for an out param, for GMP mostly, but we'll support any type.
    static inline void st_reverse_low_bits(const T& value, size_t bits, T& out) {
        // Test for zero.
        out = 0;
        if (bits == 0) { return; }

        // Reverse and shift by the correct amount.
        if constexpr(BuiltinIntegral<T>) {
            out = st_bit_reverse_full(value);
            constexpr size_t WIDTH = sizeof(T) * 8;
            out >>= (WIDTH - bits);
        } else if constexpr(GMPIntegral<T>) {
            for (size_t bit = 0; bit < bits; bit++) {
                if (mpz_tstbit(value.get_mpz_t(), bit)) {
                    mpz_setbit(out.get_mpz_t(), bits - 1 - bit);
                }
            }
        }
    }



    //
    // Log2 for Integers (Most Significant Bit Based)
    // The std::log2() returns double precision which only affords 53 bits for integrer portion.  This means it doesn't even work
    // for uint64_t.  GMP also has its own means for log2 (sizeinbase()).  Int 128 would fail too.
    //
    // To resolve this we make our own method.
    static inline size_t floor_log2(const T& val) {
        if (val == 0) { return 0; }
        size_t result = 0;
        if constexpr(NativeIntegral<T>) {
            result = 63 - __builtin_clzll(val);
        } else if constexpr(ExtendedIntegral<T>) {
            if (val >> 64) {
                result = 127 - __builtin_clzll((uint64_t)(val >> 64));
            } else {
                result = 63 - __builtin_clzll((uint64_t)val);
            }
        } else if constexpr(GMPIntegral<T>) {
            result = mpz_sizeinbase(val.get_mpz_t(), 2) - 1;
        } else {
            throw std::logic_error("Unknown type.");
        }
        return result;
    }



    //
    // Node Level
    // Calculate node level.  Since they grow dyadically, we just need a log2(N) essentially, but have to respect the root node
    // and the subsequent offset.
    //
    // Formula: floor(log2(N+Offset)) + 1
    static inline size_t st_node_level(const T& value) {
        size_t level = 0;
        if constexpr(BuiltinIntegral<T>) {
            level = floor_log2(value + _offset) + 1;
        } else if constexpr(GMPIntegral<T>) {
            // Adding 1 is a waste of alloc here, so use a scratch variable.
            static thread_local mpz_class junk = 0;
            mpz_add_ui(junk.get_mpz_t(), value.get_mpz_t(), _offset);
            level = floor_log2(junk) + 1;
        } else {
            throw std::logic_error("Unknown type.");
        }
        return level;
    }



    //
    // Position
    // Calculate the position from left to right, using 1-based index, of any node value.
    //
    // Formula: bit_reverse_L((N+Offset) % 2^(L-1), (L-1)) + 1   Where L = floor(log2(N+1))
    static inline T st_node_position(const T& value) {
        size_t level = BinaryTreeMath<T>::st_node_level(value);
        T value_plus_offset = value + _offset;
        T low_bits;
        static thread_local T position;

        // Get lower bits.
        if constexpr(BuiltinIntegral<T>) {
            low_bits = value_plus_offset & ((T(1) << (level - 1)) - 1);
        } else if constexpr(GMPIntegral<T>) {
            mpz_class mask = (mpz_class(1) << (level - 1)) - 1;
            low_bits = value_plus_offset & mask;
        } else {
            throw std::logic_error("Unknown type.");
        }

        // Get the position now.
        position = BinaryTreeMath<T>::st_reverse_low_bits(low_bits, level - 1);
        if constexpr(BuiltinIntegral<T>) {
            position += 1;
        } else if constexpr(GMPIntegral<T>) {
            mpz_add_ui(position.get_mpz_t(), position.get_mpz_t(), 1);
        } else {
            throw std::logic_error("Unknown type.");
        }

        return position;
    }



    //
    // Step
    // Trees grow in a consistent manner, making the "step" calculation easy based on level.  The "level" should be the current
    // level of the tree you're stepping away from.  E.g.: parent level when building children.
    //
    // Formula: 2^(level - 1)
    static inline T st_step(size_t level) {
        static thread_local T step;
        if constexpr(BuiltinIntegral<T>) {
            step = T(1) << (level - 1);
        } else if constexpr(GMPIntegral<T>) {
            mpz_pow_ui(step.get_mpz_t(), CollatzConstants::MPZ_TWO.get_mpz_t(), (level - 1));
        } else {
            throw std::logic_error("Unknown type.");
        }
        return step;
    }



    //
    // Max Node Value at Level
    // Calculate max node value at a level.
    //
    // Formula: 2^(level) - 1 - Offset.
    static inline mpz_class st_max_node_value_at_level(size_t level) {
        static thread_local mpz_class max_n;
        mpz_pow_ui(max_n.get_mpz_t(), _MPZ_TWO.get_mpz_t(), level);
        mpz_sub_ui(max_n.get_mpz_t(), max_n.get_mpz_t(), 1);
        mpz_sub_ui(max_n.get_mpz_t(), max_n.get_mpz_t(), _offset);
        return max_n;
    }



    //
    // Max Full Level of Node
    // The highest full level of a BinaryTree we can build with node N as the highest initial value before failure/overflow.
    //
    // Formula: level(max_iv + 1) - 1.
    //    i.e.: level of <failure point> - 1 == safest level we can fully build.
    //
    // We can just use node level of max_iv + 1 (the failure point) and subtract one level.
    static inline size_t st_max_full_level_at_node(T max_iv) {
        if (max_iv == 0) { return 0; }
        static thread_local T failure_point;
        failure_point = max_iv + 1;
        return st_node_level(failure_point) - 1;
    }



    //
    // Level Will Fit
    // Determine if the level requested is going to fit within the bit-size of T.
    //
    // This method used to be more complex because type T was limited by peak_by_bit as we scanned through a Collatz sequence to
    // build the Node's FG chain (inside Node::init()).  This has been changed to auto-promote the key pieces to avoid overflow
    // without affecting Node's T type.  The result is we can build levels up to bit_width(T) - 1.
    static inline bool st_level_will_fit(size_t level) {
        if constexpr(BuiltinIntegral<T>) {
            return level <= BinaryTreeMath<T>::st_max_level_of_type();
        }
        return true;
    }



    //
    // Max Level of Type
    // Reads type T to determine the maximum level supported.
    static inline size_t st_max_level_of_type() {
        // Max level is scaled with tree size (diadic) but must respect implementation limits, ergo:
        //   Bit width
        //   Minus 1 because the last node on 2^bit level overflows due to counting starting at 0.
        //   Minus 1 if the type is signed.
        size_t bits = sizeof(T) * 8;
        size_t max_level = bits - 1 - (std::is_signed_v<T> ? 1 : 0);
        return max_level;
    }



    //
    // First Node Value
    // The value of the first node of any level.
    //
    // Formula: 2^(level - 1) - Offset
    static inline T st_first_node_of_level(const size_t level) {
        static thread_local T first_node_value;
        if constexpr(BuiltinIntegral<T>) {
            first_node_value = (T(1) << (level - 1)) - _offset;
        } else if constexpr(std::same_as<T, mpz_class>) {
            mpz_pow_ui(first_node_value.get_mpz_t(), _MPZ_TWO.get_mpz_t(), (level - 1));
            mpz_sub_ui(first_node_value.get_mpz_t(), first_node_value.get_mpz_t(), _offset);
        } else {
            throw std::logic_error("Unknown type.");
        }
        return first_node_value;
    }



    //
    // S1 Value
    // The summation of increases when trying to find a node value at a given position.  Since it's a linear series, we can create
    // an algebraic expression for it.  See formula.
    //
    // Note: This is an OLD technique using summation, used by: st_node_value_by_position_and_level__deprecated().
    //
    // Formula: 1     * ceil((P - 1) / 2) * (2^(L-2)) [* 1]
    //    i.e.: Value * Quantity          * Scale
    static inline T st_s1_summation(const T& position, size_t level) {
        static thread_local T value = 1;
        static thread_local T quantity;
        static thread_local T scale;

        // Leave unless the level is high enough to have any quantity.  AKA: L > 1
        if (level < 2) { return T(0); }

        // Quantity: ceil((pos - 1) / 2)
        // We can simplify because: ceil((P - 1) / 2) ==> ((P - 1)+1)/2 ==> pos / 2
        quantity = position / 2;

        // Scale: 2^(L-2)
        if constexpr(BuiltinIntegral<T>) {
            scale = T(1) << (level - 2);
        } else if constexpr(GMPIntegral<T>) {
            mpz_pow_ui(scale.get_mpz_t(), _MPZ_TWO.get_mpz_t(), (level - 2));
        } else {
            throw std::logic_error("Unknown type.");
        }

        // Result of multiplication should be correctly typed.
        return value * quantity * scale;
    }



    //
    // S2 Value
    // The summation of decreases when trying to find a node value at a given position.  Requires a summation (loop).
    //
    // Note: This is an OLD technique using summation, used by: st_node_value_by_position_and_level__deprecated().
    //
    // Formula: [a=3, to L=level] 𝝨 -(2^(a-1) - 3) * ceil((P - 2^(a-2)) / 2^(a-1)) * 2^(L-a)
    //    i.e.: for 3 to L    sum(  Value          * Quantity                      * Scale)
    //
    // NOTE!  We return S2 as a positive value since we use unsigned data types!  Subtract it when you use it!!
    static inline T st_s2_summation(const T& position, size_t level) {
        static thread_local T quantity;
        static thread_local T value;
        static thread_local T scale;
        static thread_local T summation;

        // Summation Loop
        summation = 0;
        for(size_t a=3; a<=level; a++) {
            // Quantity: ceil((P - 2^(a-2)) / 2^(a-1))
            // Again we can cheat because ceiling behaves this way:
            //   ceil(x/y) ==> (x + y - 1) / y
            //
            // In our case we have the following:
            //   x (numerator)  : P - 2^(a-2)
            //   y (denominator): 2^(a-1)
            //   ceiling(x/y)   : (x + y - 1) / y
            //
            // With ceiling applied it looks like this:
            //   (P - 2^(a-2) + 2^(a-1) - 1) / 2^(a-1)
            //
            // The first subtraction can be rewritten as addition of a negative:
            //   (P + (-2^(a-2)) + 2^(a-1) - 1) / 2^(a-1)
            //
            // Now we isolate the powers of two sections in our numerator to see how they simplify:
            //   -2^(a-2) + 2^(a-1)
            //   ==> -2^(a-2) + 2 * 2^(a-2)
            //   ==> -2^(a-2) + 2^(a-2) + 2^(a-2)
            //   ==> 2^(a-2)
            //
            // The full numerator is now simplified to:
            //   P + 2^(a-2) - 1
            //
            // The full expression is now just:
            //   (P + 2^(a-2) - 1) / 2^(a-1)
            //
            // We can now simply bit shift a couple areas and be done:
            //   (P + (T(1) << (a - 2)) - 1) >> (a - 1)
            quantity = (position + (T(1) << (a - 2)) - 1) >> (a - 1);

            // Value: -(2^(a-1) - 3)
            // Remember to use a positive value, even though it's technically a decreasing summation (negative).
            if constexpr(BuiltinIntegral<T>) {
                value = (T(1) << (a - 1)) - 3;
            } else if constexpr(GMPIntegral<T>) {
                mpz_pow_ui(value.get_mpz_t(), _MPZ_TWO.get_mpz_t(), (a - 1));
                mpz_sub_ui(value.get_mpz_t(), value.get_mpz_t(), 3);
            } else {
                throw std::logic_error("Unknown type.");
            }

            // Scale: 2^(L-a)
            if constexpr(BuiltinIntegral<T>) {
                scale = T(1) << (level - a);
            } else if constexpr(GMPIntegral<T>) {
                mpz_pow_ui(scale.get_mpz_t(), _MPZ_TWO.get_mpz_t(), (level - a));
            } else {
                throw std::logic_error("Unknown type.");
            }

            // Add to Summation
            summation += (quantity * value * scale);
        }

        // Return it.
        return summation;
    }



    //
    // Node Value by Position and Level
    // Calculate a node's value by its position and level.  It supersedes the deprecated version following it.
    //
    // Formula: 2^(L-1)  + bit_reverse_L(pos - 1, L-1) - Offset
    //    i.e.: Lift     + Index                       - Offset
    static inline T st_node_value_by_position_and_level(const T& position, size_t level) {
        static thread_local T lift;
        static thread_local T index;

        // Lift: 2^(L-1)
        if constexpr(BuiltinIntegral<T>) {
            lift = Exponents::get_power_of_two<T>(level - 1);
            lift = T(1) << (level - 1);
        } else if constexpr(GMPIntegral<T>) {
            mpz_pow_ui(lift.get_mpz_t(), _MPZ_TWO.get_mpz_t(), (level - 1));
        } else {
            throw std::logic_error("Unknown type.");
        }

        // New Position: bit_reverse_L(pos - 1)
        if constexpr(BuiltinIntegral<T>) {
            index = position - 1;
        } else if constexpr(GMPIntegral<T>) {
            mpz_sub_ui(index.get_mpz_t(), position.get_mpz_t(), 1);
        } else {
            throw std::logic_error("Unknown type.");
        }

        // Reverse the low bits.  For GMP, use an out param to avoid alloc (yes, even with TLS).
        if constexpr(BuiltinIntegral<T>) {
            index = st_reverse_low_bits(index, (level - 1));
        } else {
            static thread_local T index_copy;
            index_copy = index;
            st_reverse_low_bits(index_copy, (level - 1), index);
        }

        // Return
        if constexpr(BuiltinIntegral<T>) {
            return lift + index - _offset;
        } else {
            static thread_local T result;
            mpz_set(result.get_mpz_t(), lift.get_mpz_t());
            mpz_add(result.get_mpz_t(), result.get_mpz_t(), index.get_mpz_t());
            mpz_sub_ui(result.get_mpz_t(), result.get_mpz_t(), _offset);
            return result;
        }
    }
    //
    // Alternate version for GMP mostly.
    static inline void st_node_value_by_position_and_level(const T& position, size_t level, T& out) {
        static thread_local T lift;
        static thread_local T index;

        // Lift: 2^(L-1)
        if constexpr(BuiltinIntegral<T>) {
            lift = Exponents::get_power_of_two<T>(level - 1);
            lift = T(1) << (level - 1);
        } else if constexpr(GMPIntegral<T>) {
            mpz_pow_ui(lift.get_mpz_t(), _MPZ_TWO.get_mpz_t(), (level - 1));
        } else {
            throw std::logic_error("Unknown type.");
        }

        // New Position: bit_reverse_L(pos - 1)
        if constexpr(BuiltinIntegral<T>) {
            index = position - 1;
        } else if constexpr(GMPIntegral<T>) {
            mpz_sub_ui(index.get_mpz_t(), position.get_mpz_t(), 1);
        } else {
            throw std::logic_error("Unknown type.");
        }

        // Reverse the low bits.  For GMP, use an out param to avoid alloc (yes, even with TLS).
        if constexpr(BuiltinIntegral<T>) {
            index = st_reverse_low_bits(index, (level - 1));
        } else {
            static thread_local T index_copy;
            index_copy = index;
            st_reverse_low_bits(index_copy, (level - 1), index);
        }

        // Return
        if constexpr(BuiltinIntegral<T>) {
            out = lift + index - _offset;
        } else {
            mpz_set(out.get_mpz_t(), lift.get_mpz_t());
            mpz_add(out.get_mpz_t(), out.get_mpz_t(), index.get_mpz_t());
            mpz_sub_ui(out.get_mpz_t(), out.get_mpz_t(), _offset);
        }
    }
    //
    // Now here's the old, deprecated form which uses summations.
    //
    // Formula: first_node_value + s1 + s2   (NOTE! we actually subtract s2, since it returns a positive, unsigned value)
    // (See st_s1... and st_s2... methods for details.)
    static inline T st_node_value_by_position_and_level__deprecated(const T& position, size_t level) {
        return st_first_node_of_level(level) + st_s1_summation(position, level) - st_s2_summation(position, level);
    }

};
