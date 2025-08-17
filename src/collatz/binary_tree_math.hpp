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

template<IntegralOrMPZClass T>
class BinaryTreeMath {
    private:
    static inline const mpz_class _MPZ_TWO = 2;
    static inline const mpf_class _MPF_TWO = 2;
    static inline const T _ROOT_VALUE_DEFAULT = 0;
    static inline T _root_value = _ROOT_VALUE_DEFAULT;
    static inline T _offset = 1 - _root_value;

    public:
    // Don't instanitate this.
    BinaryTreeMath() {
        throw std::logic_error("The BinaryTreeMath class isn't meant to be instanitated.");
    }



    //
    // Getter and Setter for Root Value
    //
    static const T get_default_root_value() { return _ROOT_VALUE_DEFAULT; }
    static const T get_root_value() { return _root_value; }
    static void reset_root_value() { _root_value = _ROOT_VALUE_DEFAULT; }
    static T get_offset() { return _offset; }
    static void set_root_value(T value) {
        if (value != 0 && value != 1) {
            throw std::out_of_range("You cannot set the BinaryTreeMath root value to anything other than 0 or 1.");
        }
        _root_value = value;
        _offset = 1 - _root_value;
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
            if constexpr(std::integral<T>) {
                if ((value >> bit) & 1) {
                    result |= (T(1) << (bits - 1 - bit));
                }
            } else {
                if (mpz_tstbit(value.get_mpz_t(), bit)) {
                    mpz_setbit(result.get_mpz_t(), bits - 1 - bit);
                }
            }
        }
        return result;
    }



    //
    // Node Level
    // Calculate node level.
    //
    // Formula: floor(log2(N+Offset))
    static inline size_t st_node_level(const T& value) {
        size_t level = 0;
        if constexpr(std::integral<T>) {
            // Integer truncation will cover us with a static cast.
            level = static_cast<size_t>(std::log2(value + _offset));
        } else if constexpr(std::same_as<T, mpz_class>) {
            // GMP doesn't have a logarithm function, but we can exploit sizeinbase() - 1 for the same.
            // Adding 1 is a waste of alloc here, so use a scratch variable.
            static thread_local mpz_class junk = 0;
            mpz_add(junk.get_mpz_t(), value.get_mpz_t(), _offset.get_mpz_t());
            level = mpz_sizeinbase(junk.get_mpz_t(), 2) - 1;
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
        if constexpr(std::integral<T>) {
            low_bits = value_plus_offset & ((T(1) << level) - 1);
        } else {
            mpz_class mask = (mpz_class(1) << level) - 1;
            low_bits = value_plus_offset & mask;
        }
        T position = BinaryTreeMath<T>::st_reverse_low_bits(low_bits, level);
        if constexpr(std::integral<T>) {
            position += 1;
        } else {
            mpz_add_ui(position.get_mpz_t(), position.get_mpz_t(), 1);
        }
        return position;
    }



    //
    // Max Position of Level
    // The maximum position that can exist for a node on a level.
    //
    // Formula: 2^level
    static inline T st_max_position_of_level(size_t level) {
        static thread_local T max_position;
        if constexpr(std::integral<T>) {
            max_position = 1ULL << level;
        } else {
            mpz_pow_ui(max_position.get_mpz_t(), _MPZ_TWO.get_mpz_t(), level);
        }
        return max_position;
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
        if constexpr(std::integral<T>) {
            mpz_sub_ui(max_n.get_mpz_t(), max_n.get_mpz_t(), _offset);
        } else {
            mpz_sub(max_n.get_mpz_t(), max_n.get_mpz_t(), _offset.get_mpz_t());
        }
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
        static thread_local T failure_point;
        failure_point = max_iv + 1;
        return st_node_level(failure_point) - 1;
    }



    // Level Will Fit
    // Determine if the level requested is going to fit within the bit-size of T.  Only applies to native integrals.
    // Leverages the CollatzConstants to know max IV for the given bit size.  IV's higher than this will overflow the bit during
    // sequence generation.
    static inline bool st_level_will_fit(size_t level) {
        if constexpr(std::integral<T>) {
            size_t bits = std::numeric_limits<T>::digits;
            T max_iv_allowed = CollatzConstants::get_max_initial_value_by_bit(bits);
            mpz_class max_iv_needed = st_max_node_value_at_level(level);
            return (max_iv_allowed >= max_iv_needed);
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
        if constexpr(std::integral<T>) {
            first_node_value = (1ULL << level) - _offset;
        } else if constexpr(std::same_as<T, mpz_class>) {
            mpz_pow_ui(first_node_value.get_mpz_t(), _MPZ_TWO.get_mpz_t(), level);
            mpz_sub(first_node_value.get_mpz_t(), first_node_value.get_mpz_t(), _offset.get_mpz_t());
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
        static thread_local float frequency_tmp_f;
        static thread_local mpf_class frequency_tmp_mpf_c;

        // Frequency: ceil((pos - 1) / 2)
        if constexpr(std::integral<T>) {
            frequency_tmp_f = position;
            frequency_tmp_f -= 1;
            frequency_tmp_f /= 2;
            frequency = std::ceil(frequency_tmp_f);
        } else {
            frequency_tmp_mpf_c = position;
            mpf_sub_ui(frequency_tmp_mpf_c.get_mpf_t(), frequency_tmp_mpf_c.get_mpf_t(), 1);
            mpf_div_ui(frequency_tmp_mpf_c.get_mpf_t(), frequency_tmp_mpf_c.get_mpf_t(), 2);
            mpf_ceil(frequency_tmp_mpf_c.get_mpf_t(), frequency_tmp_mpf_c.get_mpf_t());
            mpz_set_f(frequency.get_mpz_t(), frequency_tmp_mpf_c.get_mpf_t());
        }

        // Value: 2^(L-1)
        if constexpr(std::integral<T>) {
            value = 1ULL << (level - 1);
        } else {
            mpz_pow_ui(value.get_mpz_t(), _MPZ_TWO.get_mpz_t(), level - 1);
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
        static thread_local float frequency_tmp_f;
        static thread_local mpf_class frequency_tmp_mpf_c;
        static thread_local mpf_class junk_mpf_c;

        // Summation Loop
        summation = 0;
        for(size_t n=2; n<level; n++) {
            // Frequency: ceil((pos - 2^(n-1)) / 2^n)
            if constexpr(std::integral<T>) {
                frequency_tmp_f = position;
                frequency_tmp_f -= (1ULL << (n - 1));
                frequency_tmp_f /= (1ULL << n);
                frequency = std::ceil(frequency_tmp_f);
            } else {
                // Assign position.
                frequency_tmp_mpf_c = position;
                // Build the 2^(n-1)
                mpf_pow_ui(junk_mpf_c.get_mpf_t(), _MPF_TWO.get_mpf_t(), (n - 1));
                // Subtract: pos - 2^(n-1)
                mpf_sub(frequency_tmp_mpf_c.get_mpf_t(), frequency_tmp_mpf_c.get_mpf_t(), junk_mpf_c.get_mpf_t());
                // Build the 2^n
                mpf_pow_ui(junk_mpf_c.get_mpf_t(), _MPF_TWO.get_mpf_t(), n);
                // Divide: (pos - 2^(n-1)) / (2^n)
                mpf_div(frequency_tmp_mpf_c.get_mpf_t(), frequency_tmp_mpf_c.get_mpf_t(), junk_mpf_c.get_mpf_t());
                // Apply ceiling.
                mpf_ceil(frequency_tmp_mpf_c.get_mpf_t(), frequency_tmp_mpf_c.get_mpf_t());
                // Save to MPZ.
                mpz_set_f(frequency.get_mpz_t(), frequency_tmp_mpf_c.get_mpf_t());
            }

            // Value: (2^n - 3)
            if constexpr(std::integral<T>) {
                value = (1ULL << n) - 3;
            } else {
                mpz_pow_ui(value.get_mpz_t(), _MPZ_TWO.get_mpz_t(), n);
                mpz_sub_ui(value.get_mpz_t(), value.get_mpz_t(), 3);
            }

            // Magnitude: 2^(L-n)
            if constexpr(std::integral<T>) {
                magnitude = 1ULL << (level - n);
            } else {
                mpz_pow_ui(magnitude.get_mpz_t(), _MPZ_TWO.get_mpz_t(), (level - n));
            }

            // Add to Summation
            summation += (frequency * value * magnitude);
        }

        // Return it.
        return summation;
    }



    //
    // Node Value by Position and Level
    // Calculate a node's value by it's position and level.  It supersedes the deprecated version following it.
    //
    // Formula: 2^L  + bit_reverse_L(pos - 1, L) - Offset
    //    i.e.: Lift + New_Position              - Offset
    static inline T st_node_value_by_position_and_level(const T& position, size_t level) {
        static thread_local T lift;
        static thread_local T new_position;

        // Lift: 2^L
        if constexpr(std::integral<T>) {
            lift = 1ULL << level;
        } else {
            mpz_pow_ui(lift.get_mpz_t(), _MPZ_TWO.get_mpz_t(), level);
        }

        // New Position: bit_reverse_L(pos - 1)
        if constexpr(std::integral<T>) {
            new_position = position - 1;
        } else {
            mpz_sub_ui(new_position.get_mpz_t(), position.get_mpz_t(), 1);
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
