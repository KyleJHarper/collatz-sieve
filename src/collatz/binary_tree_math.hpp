#pragma once
#include <gmp.h>
#include <stdexcept>
#include <cmath>
#include "concepts.hpp"
#include "collatz.hpp"

//
// The BinaryTree class could maintain all of this, but we might want to seprate the math from the tree itself, even though the
// tree will intrinsically build (via add_level()) according to this math.
//
// This also allows other classes or units to leverage the math without needing BinaryTree.
// That said, I can't imagine a time (outside of circular dependecy hell like Tree->Node->Tree) where you'd use this.
// Ergo, much of this will be exposed with BinaryTree too in wrapper methods.
//
// A Note About Indexs and Positions
//     - Levels are the vertical coordinate of the tree and they are labeled with the counting numbers: 1, 2, 3...
//     - Positions are the horizontal coordinate of the tree and they are labeled with the counting numbers: 1, 2, 3...
// This means levels and positions are quantities by default, not zero-based indexes.  We use unordered maps internally iwth level
// as the key.  This means you can safely iterate keys or iterated *if* you start with i=1, not i=0.



//
// LevelInterval
//
// Implicit trees leverage NodeCoordinates by converting them to ranges that can be skipped.  That's what this structure holds.
//
template<AnySupportedIntegral T>
struct Interval {
    // Disallow sizes smaller than 16 bits to avoid a GCC bug when used in vectors.
    using storage_t = std::conditional_t<
        (sizeof(T) < sizeof(uint16_t)),
        uint16_t,
        T
    >;
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
    // Bit Reversal
    // The nature of the binary tree structure means certain liberties can be taken when calculating positions or node values
    // within the tree, largely due to the constant power-of-two increase.  In order leverage this, we need to be able to take bits
    // from a node value or position and reverse them, but only a certain number of them on the LSB (least significant bit) side.
    // There are a lot of ways to do this, and some compilers have intrisics for this, but I'm going to use the loop meethod to
    // keep the intention clear.  We can replace it with a black-box function later
    //
    // Formula: reverse(bits)[0:size]
    static inline T st_reverse_low_bits(const T& value, size_t bits) {
        static thread_local T result;
        result = 0;
        if (bits == 0) { return result; }

        // No built-in and/or non-integral.  Use a loop.
        for (size_t bit = 0; bit < bits; bit++) {
            if constexpr(BuiltinIntegral<T>) {
                if ((value >> bit) & 1) {
                    result |= (T(1) << (bits - 1 - bit));
                }
            } else if constexpr(GMPIntegral<T>) {
                if (mpz_tstbit(value.get_mpz_t(), bit)) {
                    mpz_setbit(result.get_mpz_t(), bits - 1 - bit);
                }
            } else {
                throw std::logic_error("Unknown type.");
            }
        }
        return result;
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
    // Calculate node level.
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
    // Formula: bit_reverse_L((N+Offset) % 2^L, L) + 1   Where L = floor(log2(N+1))
    static inline T st_node_position(const T& value) {
        size_t level = BinaryTreeMath<T>::st_node_level(value);
        T value_plus_offset = value + _offset;
        T low_bits;

        // Get lower bits.
        if constexpr(BuiltinIntegral<T>) {
            low_bits = value_plus_offset & ((T(1) << level) - 1);
        } else if constexpr(GMPIntegral<T>) {
            mpz_class mask = (mpz_class(1) << level) - 1;
            low_bits = value_plus_offset & mask;
        } else {
            throw std::logic_error("Unknown type.");
        }

        // Get the position now.
        T position = BinaryTreeMath<T>::st_reverse_low_bits(low_bits, level);
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
    // Formula: 2^(level+1) - 1 - Offset.
    static inline mpz_class st_max_node_value_at_level(size_t level) {
        static thread_local mpz_class max_n;
        mpz_pow_ui(max_n.get_mpz_t(), _MPZ_TWO.get_mpz_t(), (level + 1));
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
    // Determine if the level requested is going to fit within the bit-size of T.  Only applies to native integrals.
    // Leverages the CollatzConstants to know max IV for the given bit size.  IV's higher than this will overflow the bit during
    // sequence generation.
    static inline bool st_level_will_fit(size_t level) {
        if constexpr(BuiltinIntegral<T>) {
            size_t bits = sizeof(T) * 8;
            T max_iv_allowed = CollatzConstants::get_max_initial_value_by_bit<T>(bits);
            mpz_class max_iv_allowed_mpz;
            if constexpr(NativeIntegral<T>) {
                max_iv_allowed_mpz = max_iv_allowed;
            } else if constexpr(ExtendedIntegral<T>) {
                uint128_to_mpz(max_iv_allowed, max_iv_allowed_mpz);
            }
            mpz_class max_iv_needed_mpz = st_max_node_value_at_level(level);
            return (max_iv_allowed_mpz >= max_iv_needed_mpz);
        }
        return true;
    }



    //
    // First Node Value
    // The value of the first node of any level.
    //
    // Formula: 2^level - Offset
    static inline T st_first_node_of_level(const size_t level) {
        static thread_local T first_node_value;
        if constexpr(BuiltinIntegral<T>) {
            first_node_value = (T(1) << level) - _offset;
        } else if constexpr(std::same_as<T, mpz_class>) {
            mpz_pow_ui(first_node_value.get_mpz_t(), _MPZ_TWO.get_mpz_t(), level);
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
    // Formula: ceil((pos - 1) / 2) * (2^(L-1)) [* 1]
    //    i.e.: Frequency           * Value   [* Magnitude, always 1]
    static inline T st_s1_summation(const T& position, size_t level) {
        static thread_local T frequency;
        static thread_local T value;

        // Frequency: ceil((pos - 1) / 2)
        // We can cheat because...: ceil((pos-1)/2) ==> ((pos-1)+1)/2 ==> pos / 2
        frequency = position / 2;

        // Value: 2^(L-1)
        if constexpr(BuiltinIntegral<T>) {
            value = T(1) << (level - 1);
        } else if constexpr(GMPIntegral<T>) {
            mpz_pow_ui(value.get_mpz_t(), _MPZ_TWO.get_mpz_t(), level - 1);
        } else {
            throw std::logic_error("Unknown type.");
        }

        // Result of multiplication should be correctly typed.
        return frequency * value;
    }



    //
    // S2 Value
    // The summation of decreases when trying to find a node value at a given position.  Requires a summation (loop).
    //
    // Note: This is an OLD technique using summation, used by: st_node_value_by_position_and_level__deprecated().
    //
    // Formula: [n=2, to L=level] 𝝨 ceil((pos - 2^(n-1)) / 2^n) * (2^n - 3) * 2^(L-n)
    //    i.e.: for 2 to L      sum(Frequency                   * Value     * Magnitude)
    static inline T st_s2_summation(const T& position, size_t level) {
        static thread_local T frequency;
        static thread_local T value;
        static thread_local T magnitude;
        static thread_local T summation;

        // Summation Loop
        summation = 0;
        for(size_t n=2; n<level; n++) {
            // Frequency: ceil((pos - 2^(n-1)) / 2^n)
            // Again we can cheat ...
            //   Numerator  : (pos - 2^(n-1))  ==>  (pos - (1 << (n-1)))
            //   Denominator: 2^n              ==>  >> n
            //   Ceiling    : ceil(x/y)        ==>  (x + y - 1) / y
            // Final: (pos - (1 << (n-1)) + (1 << n) - 1) >> n
            frequency = (position - (T(1) << (n - 1)) + (T(1) << n) - 1) >> n;

            // Value: (2^n - 3)
            if constexpr(BuiltinIntegral<T>) {
                value = (T(1) << n) - 3;
            } else if constexpr(GMPIntegral<T>) {
                mpz_pow_ui(value.get_mpz_t(), _MPZ_TWO.get_mpz_t(), n);
                mpz_sub_ui(value.get_mpz_t(), value.get_mpz_t(), 3);
            } else {
                throw std::logic_error("Unknown type.");
            }

            // Magnitude: 2^(L-n)
            if constexpr(BuiltinIntegral<T>) {
                magnitude = T(1) << (level - n);
            } else if constexpr(GMPIntegral<T>) {
                mpz_pow_ui(magnitude.get_mpz_t(), _MPZ_TWO.get_mpz_t(), (level - n));
            } else {
                throw std::logic_error("Unknown type.");
            }

            // Add to Summation
            summation += (frequency * value * magnitude);
        }

        // Return it.
        return summation;
    }



    //
    // Node Value by Position and Level
    // Calculate a node's value by its 1-based position and level.  It supersedes the deprecated version following it.
    //
    // Formula: 2^L  + bit_reverse_L(pos - 1, L) - Offset
    //    i.e.: Lift + New_Position              - Offset
    static inline T st_node_value_by_position_and_level(const T& position, size_t level) {
        static thread_local T lift;
        static thread_local T new_position;

        // Lift: 2^L
        if constexpr(BuiltinIntegral<T>) {
            lift = T(1) << level;
        } else if constexpr(GMPIntegral<T>) {
            mpz_pow_ui(lift.get_mpz_t(), _MPZ_TWO.get_mpz_t(), level);
        } else {
            throw std::logic_error("Unknown type.");
        }

        // New Position: bit_reverse_L(pos - 1)
        if constexpr(BuiltinIntegral<T>) {
            new_position = position - 1;
        } else if constexpr(GMPIntegral<T>) {
            mpz_sub_ui(new_position.get_mpz_t(), position.get_mpz_t(), 1);
        } else {
            throw std::logic_error("Unknown type.");
        }
        new_position = st_reverse_low_bits(new_position, level);

        // Return
        return lift + new_position - _offset;
    }
    //
    // Now here's the old, deprecated form which uses summations.
    //
    // Formula: first_node_value + s1 - s2
    // (See st_s1... and st_s2... methods for details.)
    static inline T st_node_value_by_position_and_level__deprecated(const T& position, size_t level) {
        return st_first_node_of_level(level) + st_s1_summation(position, level) - st_s2_summation(position, level);
    }

};
