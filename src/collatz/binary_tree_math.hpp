#pragma once
#include <gmp.h>
#include <limits>
#include <stdexcept>
#include <cmath>
#include "concepts.hpp"
#include "bit.hpp"
#include "collatz_constants.hpp"
#include "exponents.hpp"




/**
* @class BinaryTreeMath
* @brief The core algorithms supporting tree building and navigation.
* @attention This class contains many of the core formulas driving the entire algorithm behind the custom `BinaryTree`.
*
* The math behind any BinaryTree must support the two types of coordinates consistently.  These are:
*   - Levels are the vertical coordinates and are labeled with countering numbers: 1, 2, 3 ...
*   - Positions are the left-to-right horizontal coordinates and are labeled with counting numbers: 1, 2, 3 ...
*
* As shown, 0-based indexes are not used for tree coordinates.  They could be, but it just sounds weird to call the first level of
* a tree "level zero".  More importantly, it also requires more +1/-1 adjustments in the functions.  Levels were zero-based
* originally, but this was changed in version 3.0.0.
*
* The math for building a `BinaryTree` supports both 0- and 1-based trees.  In the original research, trees rooted at zero were
* fine because manual research was still underway into patterns.  Patterns aligned more cleanly with a tree rooted at one, and it
* also meant the tree contained the space of Collatz Conjecture correctly (no zero).  Therefore, 1-based trees became the defaul as
* of version 2.0.0.  That said, 0-based trees are still possible by changing `BinaryTreeMath::set_root_value(0)`, however their
* performance and behavior is provided on a best-effort basis and might be removed later.
*
* @tparam T Any supported integral (see concepts.hpp).
*/
template<AnySupportedIntegral T>
class BinaryTreeMath {
    private:
    /// @brief Static 2 for GMP operations to prevent allocs.
    static inline const mpz_class _MPZ_TWO = 2;
    /// @brief The default root value, which is 1 as of version 2.0.0.
    static inline const size_t _ROOT_VALUE_DEFAULT = 1;
    /// @brief The currently selected root value.  This is a program-wide static, so be careful.
    static inline size_t _root_value = _ROOT_VALUE_DEFAULT;
    /// @brief An offset which helps the functions shift their logic for 0- and 1-based roots without a bunch of if/else garbage.
    static inline size_t _offset = 1 - _root_value;



    public:
    /// @name Lifecycle Management
    /// @{

    /**
    * @brief Default constructor to throw an error since this is a static class.
    * @warning This class is not meant to be instantiated, and will throw an error if you try.
    */
    BinaryTreeMath() {
        throw std::logic_error("The BinaryTreeMath class isn't meant to be instanitated.");
    }

    /// @}



    /// @name Accessors
    /// @{

    /// @brief Get the default root value, which is 1 as of version 2.0.0.
    static size_t get_default_root_value() { return _ROOT_VALUE_DEFAULT; }
    /// @brief Get the current root value.  This is a static/program-wide setting.
    static size_t get_root_value() { return _root_value; }
    /// @brief Get the offset from _root_value, which is always `1 - _root_value`, and is updated with calls to `set_root_value()`.
    static size_t get_offset() { return _offset; }
    /// @brief Returns `_root_value` to the default of 1 (after 2.0.0).
    static void reset_root_value() {
        _root_value = _ROOT_VALUE_DEFAULT;
        _offset = 1 - _root_value;
    }
    /// @brief Sets the root value and enforces it be 0 or 1.  Anything else throws an error.
    static void set_root_value(size_t value) {
        if (value != 0 && value != 1) {
            throw std::out_of_range("You cannot set the BinaryTreeMath root value to anything other than 0 or 1.");
        }
        _root_value = value;
        _offset = 1 - _root_value;
    }

    /// @}


    /// @name Node Counts
    /// @{

    /**
    * @brief The number of nodesof a specific level on the tree.
    *
    * \par Formula
    * \f$ 2^{level - 1} \f$
    *
    * @param level The level to get node count for.
    * @return The count of nodes, typed to your `T`.
    */
    static inline T st_node_count_of_level(level_t level) {
        static thread_local T count;
        if constexpr(FixedWidthIntegral<T>) {
            count = T(1) << (level - 1);
        } else if constexpr(GMPIntegral<T>) {
            mpz_pow_ui(count.get_mpz_t(), CollatzConstants::MPZ_TWO.get_mpz_t(), (level - 1));
        }
        return count;
    }



    /**
    * @brief The number of nodes between two levels, inclusive.
    *
    * \par Formula
    * \f$ (2^{max\_level} - 1) - (2^{min\_level} - 1) \f$
    *
    * @param min_level Minimum level to count nodes on.  Inclusive.
    * @param max_level Minimum level to count nodes on.  Inclusive.
    * @return Number of nodes, typed to your `T`.
    */
    static inline T st_node_count_of_levels(level_t min_level, level_t max_level) {
        static thread_local T full_tree_count;
        static thread_local T sub_tree_count;
        static thread_local T final_count;
        if constexpr(FixedWidthIntegral<T>) {
            final_count = (T(1) << max_level) - (T(1) << (min_level - 1));
        } else if constexpr(GMPIntegral<T>) {
            mpz_pow_ui(full_tree_count.get_mpz_t(), CollatzConstants::MPZ_TWO.get_mpz_t(), max_level);
            mpz_pow_ui(sub_tree_count.get_mpz_t(), CollatzConstants::MPZ_TWO.get_mpz_t(), min_level - 1);
            mpz_sub(final_count.get_mpz_t(), full_tree_count.get_mpz_t(), sub_tree_count.get_mpz_t());
        }
        return final_count;
    }



    /**
    * @brief The number of nodes in a tree of level L depth, summing all levels.  Inclusive.
    *
    * \par Formula
    * \f$ 2^{level} - 1\f$
    *
    * @param levels Number of levels in the tree.
    * @return Count of nodes, typed to your `T`.
    */
    static inline T st_node_count_of_tree(level_t levels) {
        static thread_local T count;
        if constexpr(FixedWidthIntegral<T>) {
            count = (T(1) << levels) - 1;
        } else if constexpr(GMPIntegral<T>) {
            mpz_pow_ui(count.get_mpz_t(), CollatzConstants::MPZ_TWO.get_mpz_t(), levels);
            mpz_sub_ui(count.get_mpz_t(), count.get_mpz_t(), 1);
        }
        return count;
    }

    /// @}



    /// @name Level Operations
    /// @{

    /**
    * @brief Calculate the level for a node from any value.
    *
    * Since trees grow dyadically, the level is essentially log2(N), but consideration has to be taken for 0- and 1-based trees.
    *
    * \par Formula
    * \f$ \left\lfloor \log_{2}(N+Offset) + 1 \right\rfloor \f$
    *
    * @param value The value to compute a level for.
    * @return The level for `value`.
    */
    static inline level_t st_get_level_by_node_value(const T& value) {
        level_t level = 0;
        if constexpr(FixedWidthIntegral<T>) {
            level = st_floor_log2(value + _offset) + 1;
        } else if constexpr(GMPIntegral<T>) {
            // Adding 1 is a waste of alloc here, so use a scratch variable.
            static thread_local mpz_class junk = 0;
            mpz_add_ui(junk.get_mpz_t(), value.get_mpz_t(), _offset);
            level = st_floor_log2(junk) + 1;
        }
        return level;
    }



    /**
    * @brief Maximum full level available given any node value.
    *
    * Calculates the highest level up to node value `N` that is fully filled.  So, if `N` lands in the middle of a level, then the
    * previous level is the last full level.
    *
    * \par Formula
    * \f$
    * \begin{aligned}
    * \text{Given: } & too\_far = N + 1 \\
    * \text{Then: }  & max\_full\_level = level\_of(too\_far) - 1
    * \end{aligned}
    * \f$
    */
    static inline level_t st_max_full_level_at_node_value(T value) {
        if (value == 0) { return 0; }
        static thread_local T failure_point;
        failure_point = value + 1;
        return st_get_level_by_node_value(failure_point) - 1;
    }



    /**
    * @brief Reads type `T` to determine the maximum level supported without overflowing.
    *
    * This calculation used to be more complex, because it took into consideration the overflow limits of all values in a Collatz
    * sequence given a starting initial value or integer width.  However, now that overflows are detected and auto-escalated within
    * the `Collatz` class, the max level is simply the bit size of the type.
    *
    * \par Formula
    * \f$ \text{Simply bits} \f$
    *
    * @return Maximum level for type `T`.  When GMP, sends `level_t` max.
    */
    static inline level_t st_max_level_of_type() {
        // Max level is scaled with tree size (diadic) but must respect implementation limits, ergo:
        //   Bit width
        //   Minus 1 because the last node on 2^bit level overflows due to counting starting at 0.
        //   Minus 1 if the type is signed.
        if (GMPIntegral<T>) {
            return std::numeric_limits<level_t>::max();
        }
        size_t bits = sizeof(T) * 8;
        level_t max_level = bits - 1 - (std::numeric_limits<T>::is_signed ? 1 : 0);
        return max_level;
    }



    /**
    * @brief Determine if the level sent will fit within the type `T` specified.
    *
    * This calculation used to be more complex, but was simplified with the overflow detection and handling inside the `Collatz`
    * class.  It ends up comparing `st_max_level_of_type()` to `level`.  That's it.
    *
    * \par Formula
    * \f$ \text{Simply: level <= max_level_of_type()} \f$
    *
    * @return True if the level will fit, false otherwise.
    */
    static inline bool st_level_will_fit(level_t level) {
        if constexpr(FixedWidthIntegral<T>) {
            return level <= BinaryTreeMath<T>::st_max_level_of_type();
        }
        return true;
    }

    /// @}



    /// @name Position and Value Operations
    /// @{

    /**
    * @brief Calculate the highest value a node can have and remain on a given level.
    *
    * \par Formula
    * \f$ 2^{level} - 1 - Offset \f$
    *
    * @param level The level to calculate maximum node value for.
    * @return The max node value for the level, typed to `T`.
    */
    static inline T st_max_node_value_at_level(level_t level) {
        if constexpr (FixedWidthIntegral<T>) {
            if (sizeof(T) * 8 < level) {
                throw std::out_of_range("Level too high for st_max_node_value_at_level");
            }
            return (T(1) << level) - 1 - _offset;
        } else if constexpr (GMPIntegral<T>) {
            static thread_local mpz_class max_n;
            mpz_pow_ui(max_n.get_mpz_t(), _MPZ_TWO.get_mpz_t(), level);
            mpz_sub_ui(max_n.get_mpz_t(), max_n.get_mpz_t(), 1);
            mpz_sub_ui(max_n.get_mpz_t(), max_n.get_mpz_t(), _offset);
            return max_n;
        }
    }



    /**
    * @brief Calculate the first node's value on any level.
    *
    * \par Formula
    * \f$ 2^{level - 1} - Offset \f$
    *
    * @param level The level to get first node value from.
    * @return The first node value at `level`, typed to `T`.
    */
    static inline T st_first_node_value_of_level(level_t level) {
        static thread_local T first_node_value;
        if constexpr(FixedWidthIntegral<T>) {
            first_node_value = (T(1) << (level - 1)) - _offset;
        } else if constexpr(GMPIntegral<T>) {
            mpz_pow_ui(first_node_value.get_mpz_t(), _MPZ_TWO.get_mpz_t(), (level - 1));
            mpz_sub_ui(first_node_value.get_mpz_t(), first_node_value.get_mpz_t(), _offset);
        }
        return first_node_value;
    }



    /**
    * @brief Calculate a node's value by its position and level.
    *
    * Due to the dyadic nature and scaling of both the tree and child values, those values can be deduced by performing operations:
    *   1. Calculate the lift, which is simply the smallest even value on level L.
    *   2. Use bit reversal on the lower L-1 bits to deduce an index shift.
    *   3. Shift by the offset since trees can be 0- or 1-based.
    *
    * \par Formula
    * \f$
    * \begin{aligned}
    * & 2^{(level-1)} && + bit\_reverse\_lower\_bits(pos-1, level-1) && - Offset
    * \\
    * & Lift          && + Index                                     && - Offset
    * \end{aligned}
    * \f$
    *
    * @param position The left-to-right position of the node.
    * @param level The level the node is on.
    * @return The value of the node at the given position and level.
    */
    static inline T st_node_value_by_position_and_level(const T& position, level_t level) {
        static thread_local T lift;
        static thread_local T index;

        // Lift: 2^(L-1)
        if constexpr(FixedWidthIntegral<T>) {
            lift = Exponents::get_power_of_two<T>(level - 1);
            lift = T(1) << (level - 1);
        } else if constexpr(GMPIntegral<T>) {
            mpz_pow_ui(lift.get_mpz_t(), _MPZ_TWO.get_mpz_t(), (level - 1));
        }

        // New Position: bit_reverse_L(pos - 1)
        if constexpr(FixedWidthIntegral<T>) {
            index = position - 1;
        } else if constexpr(GMPIntegral<T>) {
            mpz_sub_ui(index.get_mpz_t(), position.get_mpz_t(), 1);
        }

        // Reverse the low bits.  For GMP, use an out param to avoid alloc (yes, even with TLS).
        if constexpr(FixedWidthIntegral<T>) {
            index = Bit::st_reverse_low_bits(index, (level - 1));
        } else if constexpr (GMPIntegral<T>) {
            static thread_local T index_copy;
            index_copy = index;
            Bit::st_reverse_low_bits(index_copy, (level - 1), index);
        }

        // Return
        if constexpr(FixedWidthIntegral<T>) {
            return lift + index - _offset;
        } else if constexpr (GMPIntegral<T>) {
            static thread_local T result;
            mpz_set(result.get_mpz_t(), lift.get_mpz_t());
            mpz_add(result.get_mpz_t(), result.get_mpz_t(), index.get_mpz_t());
            mpz_sub_ui(result.get_mpz_t(), result.get_mpz_t(), _offset);
            return result;
        }
    }



    /**
    * @brief Calculate a node's value by its position and level (out-param version).
    *
    * Due to the dyadic nature and scaling of both the tree and child values, those values can be deduced by performing operations:
    *   1. Calculate the lift, which is simply the smallest even value on level L.
    *   2. Use bit reversal on the lower L-1 bits to deduce an index shift.
    *   3. Shift by the offset since trees can be 0- or 1-based.
    *
    * \par Formula
    * \f$
    * \begin{aligned}
    * & 2^{(level-1)} && + bit\_reverse\_lower\_bits(pos-1, level-1) && - Offset
    * \\
    * & Lift          && + Index                                     && - Offset
    * \end{aligned}
    * \f$
    *
    * @param position The left-to-right position of the node.
    * @param level The level the node is on.
    * @param out The value of the node at the given position and level, stored in your `out` ref.
    */
    static inline void st_node_value_by_position_and_level(const T& position, level_t level, T& out) {
        static thread_local T lift;
        static thread_local T index;

        // Lift: 2^(L-1)
        if constexpr(FixedWidthIntegral<T>) {
            lift = Exponents::get_power_of_two<T>(level - 1);
            lift = T(1) << (level - 1);
        } else if constexpr(GMPIntegral<T>) {
            mpz_pow_ui(lift.get_mpz_t(), _MPZ_TWO.get_mpz_t(), (level - 1));
        }

        // New Position: bit_reverse_L(pos - 1)
        if constexpr(FixedWidthIntegral<T>) {
            index = position - 1;
        } else if constexpr(GMPIntegral<T>) {
            mpz_sub_ui(index.get_mpz_t(), position.get_mpz_t(), 1);
        }

        // Reverse the low bits.  For GMP, use an out param to avoid alloc (yes, even with TLS).
        if constexpr(FixedWidthIntegral<T>) {
            index = Bit::st_reverse_low_bits(index, (level - 1));
        } else if constexpr(GMPIntegral<T>) {
            static thread_local T index_copy;
            index_copy = index;
            Bit::st_reverse_low_bits(index_copy, (level - 1), index);
        }

        // Return
        if constexpr(FixedWidthIntegral<T>) {
            out = lift + index - _offset;
        } else if constexpr(GMPIntegral<T>) {
            mpz_set(out.get_mpz_t(), lift.get_mpz_t());
            mpz_add(out.get_mpz_t(), out.get_mpz_t(), index.get_mpz_t());
            mpz_sub_ui(out.get_mpz_t(), out.get_mpz_t(), _offset);
        }
    }



    /**
    * @brief Calculate node position from a node's value alone.
    *
    * This is the complement to getting a node level by value.  It uses similar bit reversal logic based on the node's value and
    * level to determine the left-to-right position the node exists on its level.  Note, it does NOT return the level for you.
    *
    * \par Formula
    * \f$
    * \begin{aligned}
    * \text{Given: } & N = node\_value
    * \\\
    * \text{Given: } & L = level\_of\_node(N)
    * \\
    * \text{Formula: } & bit\_reverse\_lower\_bits((N+Offset) \mod 2^{(L-1)}, L-1) + 1
    * \end{aligned}
    * \f$
    *
    * @param value The value of the node whose position is calculated.
    * @return The position for the node, typed to `T`.
    */
    static inline T st_node_position(const T& value) {
        level_t level = BinaryTreeMath<T>::st_get_level_by_node_value(value);
        T value_plus_offset = value + _offset;
        T low_bits;
        static thread_local T position;

        // Get lower bits.
        if constexpr(FixedWidthIntegral<T>) {
            low_bits = value_plus_offset & ((T(1) << (level - 1)) - 1);
        } else if constexpr(GMPIntegral<T>) {
            mpz_class mask = (mpz_class(1) << (level - 1)) - 1;
            low_bits = value_plus_offset & mask;
        }

        // Get the position now.
        position = Bit::st_reverse_low_bits(low_bits, level - 1);
        if constexpr(FixedWidthIntegral<T>) {
            position += 1;
        } else if constexpr(GMPIntegral<T>) {
            mpz_add_ui(position.get_mpz_t(), position.get_mpz_t(), 1);
        }

        return position;
    }



    /**
    * @brief Sums the increases from the `S1` sequence described in Harper's research.
    *
    * The S1 value is the summation of the linear series of positive jumps when looking at the deltas between the nodes' value from
    * left-to-right.  Since it's linear, simple algebra creates a clean closed-form expression using what Harper labeled the Value,
    * Quantity, and Scale.  This is discussed further in his research paper.
    *
    * \par Formula
    * \f$
    * \begin{aligned}
    * \text{Example: }    & 1     && * \left\lceil (pos - 1) / 2 \right\rceil && * (2^{level-2})
    * \\
    * \text{Components: } & Value && * Quantity                               && * Scale
    * \end{aligned}
    * \f$
    *
    * @deprecated While mathematically correct, this calculation is deprecated because the `st_node_value_by_position_and_level()`
    * can be calculated using bit twiddling much faster.
    * @param position The left-to-right position of the node on the level.
    * @param level The level the node is on.
    * @return The summation of the S1 series, returned as type `T`.
    */
    [[deprecated("Calculates positive S1 series correctly, but the whole approach is superseded by bitwise operations")]]
    static inline T st_s1_summation(const T& position, level_t level) {
        static thread_local T value = 1;
        static thread_local T quantity;
        static thread_local T scale;

        // Leave unless the level is high enough to have any quantity.  AKA: L > 1
        if (level < 2) { return T(0); }

        // Quantity: ceil((pos - 1) / 2)
        // We can simplify because: ceil((P - 1) / 2) ==> ((P - 1)+1)/2 ==> pos / 2
        quantity = position / 2;

        // Scale: 2^(L-2)
        if constexpr(FixedWidthIntegral<T>) {
            scale = T(1) << (level - 2);
        } else if constexpr(GMPIntegral<T>) {
            mpz_pow_ui(scale.get_mpz_t(), _MPZ_TWO.get_mpz_t(), (level - 2));
        }

        // Result of multiplication should be correctly typed.
        return value * quantity * scale;
    }



    /**
    * @brief Sums the decreases from the `S2` sequence described in Harper's research.
    *
    * The S2 value is the summation of the polynomial series of negative jumps when looking at the deltas between the nodes' value
    * from left-to-right.  Due to the non-linear nature of the summation, it requires looping to accomodate changes to the core
    * components in what Harper labeled the Value, Quantity, and Scale.  This is discussed further in his research paper.
    *
    * \par Formula
    * \f$
    * \begin{aligned}
    * \text{Formula: } & \sum_{a=3}^{L} = && -(2^{(a-1)} - 3) && * \left\lceil (pos - 2^{(a-2)}) / 2^{(a-1)} \right\rceil && * 2^{(L-a)}
    * \\
    * \text{Components: } &               && Value            && * Quantity                                               && * Scale
    * \end{aligned}
    * \f$
    *
    * @deprecated While mathematically correct, this calculation is deprecated because the `st_node_value_by_position_and_level()`
    * can be calculated using bit twiddling much faster.
    * @note Additional details about simplification of the `Quantity` factor are in the comments of this function.
    * @warning The `S2` summation is mathematically negative, but `T` is usually unsigned, so it's returned positive.  Be sure to
    * subtract `S2`, not add it.
    * @param position The left-to-right position of the node on the level.
    * @param level The level the node is on.
    * @return The summation of the S2 series, returned as type `T`, and signed positively.
    */
    [[deprecated("Calculates negative S2 series correctly, but the whole approach is superseded by bitwise operations")]]
    static inline T st_s2_summation(const T& position, level_t level) {
        static thread_local T quantity;
        static thread_local T value;
        static thread_local T scale;
        static thread_local T summation;

        // Summation Loop
        summation = 0;
        for(level_t a=3; a<=level; a++) {
            // Quantity: ceil((P - 2^(a-2)) / 2^(a-1))
            // Simplification exists because ceiling behaves this way:
            //   ceil(x/y) ==> (x + y - 1) / y
            //
            // Currently the formula contains:
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
            // Now it isolate the powers of two sections in the numerator to see how they simplify:
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
            // Some arithmetic and bit shifting covers the rest:
            //   (P + (T(1) << (a - 2)) - 1) >> (a - 1)
            quantity = (position + (T(1) << (a - 2)) - 1) >> (a - 1);

            // Value: -(2^(a-1) - 3)
            // Remember to use a positive value, even though it's technically a decreasing summation (negative).
            if constexpr(FixedWidthIntegral<T>) {
                value = (T(1) << (a - 1)) - 3;
            } else if constexpr(GMPIntegral<T>) {
                mpz_pow_ui(value.get_mpz_t(), _MPZ_TWO.get_mpz_t(), (a - 1));
                mpz_sub_ui(value.get_mpz_t(), value.get_mpz_t(), 3);
            }

            // Scale: 2^(L-a)
            if constexpr(FixedWidthIntegral<T>) {
                scale = T(1) << (level - a);
            } else if constexpr(GMPIntegral<T>) {
                mpz_pow_ui(scale.get_mpz_t(), _MPZ_TWO.get_mpz_t(), (level - a));
            }

            // Add to Summation
            summation += (quantity * value * scale);
        }

        // Return it.
        return summation;
    }



    /**
    * @brief Calculate a node's value by its position and level.
    * @deprecated This uses the `st_s1_summation()` and `st_s2_summation()` methods, all of which are deprecated.
    *
    * \par Formula
    * \f$ first\_node\_value + S1 - S2 \f$
    *
    * @param position The left-to-right position of the node.
    * @param level The level the node is on.
    * @return The value of the node at the given position and level.
    */
    [[deprecated("Uses old S1 and S2 formulas which are slow")]]
    static inline T st_node_value_by_position_and_level__deprecated(const T& position, level_t level) {
        return st_first_node_value_of_level(level) + st_s1_summation(position, level) - st_s2_summation(position, level);
    }

    /// @}



    /**
    * @brief Returns the FG chain length of a node on any level.
    *
    * This used to be more complicated, but now it's proven to just be: level - 1.
    *
    * \par Formula
    * \f$ level - 1 \f$
    *
    * @param level The level of the node in question.
    * @return The number of FG links in the chain at the given level.
    */
    static inline seq_size_t st_fg_chain_length(level_t level) {
        return level - 1;
    }



    /**
    * @brief A log base-2 for integers (most significant bit based).
    *
    * The std::log2() returns double precision which only affords 53 bits for integer portion.  This means it doesn't even work for
    * uint64_t.  GMP also has its own means for log2 (`sizeinbase()`).  This method uses `CLZ` functions to overcome this.
    *
    * @param val The value to operate on.
    * @return The integer of floor(log2(val)).
    */
    static inline size_t st_floor_log2(const T& val) {
        if (val == 0) { return 0; }
        size_t result = 0;
        if constexpr(FixedWidthIntegral<T>) {
            if constexpr(sizeof(T) < 64) {
                result = 63 - __builtin_clzll(val);
            } else if constexpr(sizeof(T) == 128) {
                if (val >> 64) {
                    result = 127 - __builtin_clzll((uint64_t)(val >> 64));
                } else {
                    result = 63 - __builtin_clzll((uint64_t)val);
                }
            }
        } else if constexpr(GMPIntegral<T>) {
            result = mpz_sizeinbase(val.get_mpz_t(), 2) - 1;
        }
        return result;
    }



    /**
    * @brief The magnitude (scaling factor `s_L`) between a parent's value and child values.
    *
    * The binary tree assigns child values by a scaling factor (step), not just `2n` and `2n+1` like a sequentially labeled tree.
    * This method helps compute that scaled value to creata `parent + scaling_factor` and `parent + 2 * scaling_factor` for the
    * children.  This also means siblings are separated by `s_L` too.
    *
    * \par Formula
    * \f$ 2^{(level-2)} \f$
    *
    * @param level The level of consideration.  When building children, it's the child's level, not the parent's.
    * @return The scaled value (step) value to apply to child values, typed to `T`.
    */
    static inline T st_scaling_factor(level_t level) {
        if (level < 2) {
            throw std::out_of_range("Cannot request a scaling factor for levels below 2.");
        }

        if constexpr(FixedWidthIntegral<T>) {
            return T(1) << (level - 2);
        } else if constexpr(GMPIntegral<T>) {
            static thread_local T scaling_factor;
            mpz_pow_ui(scaling_factor.get_mpz_t(), CollatzConstants::MPZ_TWO.get_mpz_t(), (level - 2));
            return scaling_factor;
        }
    }



    /**
    * @brief The cumulative magnitude required to scale a parent node `N` to its descendants on the target level.
    *
    * When a given node `N` is singled out, it creates a subtree.  To find all the leaf nodes on level `L` that are rooted by node
    * `N`, three things are required:
    *
    * 1. The cumulative magnitude to reach the smallest leaf node.  This function provides that.
    * 2. The scaling factor to iterate over leaf nodes in order.  The `st_scaling_factor()` provides that.
    * 3. The number of leaf nodes, which is simply 2^(target_level - base_level)
    *
    * Therefore, this function let's you find all the first (smallest) values of the leaf nodes on a `target_level` of any position
    * on `base_level`.  From there, you simply iterate by the scaling factor.
    *
    * \par Formula
    * \f$ 2^{(target\_level - 1)} - 2^{(base\_level - 1)} \f$
    *
    * @note This function is used with node VALUES, not POSITIONS.  Position arithmetic differs.
    * @param base_level The level to start from (where the parent node resides).
    * @param target_level The level to end at, where leaf nodes reside.
    * @return The cumulative scaling factor typed to `T`.
    */
    static inline T st_scaling_factor_cumulative(level_t base_level, level_t target_level) {
        if (target_level <= base_level) {
            throw std::out_of_range("Cannot scale a position negatively or to the same level.");
        }

        if constexpr(FixedWidthIntegral<T>) {
            return (T(1) << (target_level - 1)) - (T(1) << (base_level - 1));
        } else if constexpr(GMPIntegral<T>) {
            static thread_local T scaling_factor;
            static thread_local T base_scaling_factor;
            mpz_pow_ui(scaling_factor.get_mpz_t(), CollatzConstants::MPZ_TWO.get_mpz_t(), (target_level - 1));
            mpz_pow_ui(base_scaling_factor.get_mpz_t(), CollatzConstants::MPZ_TWO.get_mpz_t(), (base_level - 1));
            mpz_sub(scaling_factor.get_mpz_t(), scaling_factor.get_mpz_t(), base_scaling_factor.get_mpz_t());
            return scaling_factor;
        }
    }



    /**
    * @brief Calculate the number of nodes a parent node will have on a target level.
    *
    * \par Formula
    * \f$ 2^{(target\_level - base_level)} \f$
    *
    * @note This is NOT the cumulative count of the subtree for a parent nodes.
    * @param base_level The level to start from (where the parent node resides).
    * @param target_level The level to end at, where leaf nodes reside.
    * @return The number of nodes a parent will have on a target level.
    */
    static inline T st_child_count_at_target_level(level_t parent_level, level_t target_level) {
        if (target_level <= parent_level) {
            throw std::out_of_range("Cannot count children at a level equal to or higher than the parent.");
        }

        if constexpr(FixedWidthIntegral<T>) {
            return T(1) << (target_level - parent_level);
        } else if constexpr(GMPIntegral<T>) {
            static thread_local T child_count;
            mpz_pow_ui(child_count.get_mpz_t(), CollatzConstants::MPZ_TWO.get_mpz_t(), (target_level - 1));
            return child_count;
        }
    }

};
