#pragma once

#include <limits>
#include <stdexcept>
#include <type_traits>
#include <string>
#include <stdint.h>
#include "concepts.hpp"
#include "string.hpp"
#include "exponents.hpp"
#include "collatz_affine_map.hpp"
#include "collatz_constants.hpp"
#include "bit.hpp"
#include "typedefs.hpp"
#include <vector>




/**
* @exception CollatzSequenceOverflow
* @brief Custom overflow exception for Collatz sequences.
*/
class CollatzSequenceOverflow : public std::runtime_error {
    public:
    explicit CollatzSequenceOverflow(const std::string& msg) : std::runtime_error(msg) {}
};




/**
* @class Collatz
* @brief A small object with metadata for a Collatz sequence.  Mostly used for its static methods.
*
* This class will, upon initialization via the constructor or `init()`, set the metadata for a Collatz sequence starting at the
* specified `_initial_value`.
*
* Originally, sequences were stored in-memory, but this blows out cache lines, and is generally useless.  The for_each iterators
* are so fast it's almost never useful to cache them.  If callers want them cached, they must do so externally, with a vector or
* similar.
*
* @tparam T Any supported integral (see concepts.hpp).
*/
template<AnySupportedIntegral T>
class Collatz {
    private:
    // Memory packing and alignment matter!  Keep this class LIGHT.  All data must fit within one cache line.
    //                                   uint64_t | total | uint128_t | total | mpz_class | total
    T _initial_value = 0;                  //        8 |     8 |        16 |    16 |        16 |    16
    T _peak_value = 0;                     //        8 |    16 |        16 |    32 |        16 |    32
    seq_size_t _hwm_index = 0;             //        4 |    20 |         4 |    36 |         4 |    36
    seq_size_t _step_count = 0;            //        4 |    24 |         4 |    40 |         4 |    40
    bool _is_initialized : 1 = false;      //      1:1 |    25 |       1:1 |    41 |       1:1 |    41
    bool _sequence_overflows : 1 = false;  //      1:2 |    25 |       1:2 |    41 |       1:2 |    41  (6 bits padding)
    // Alignment Padding                   //        7 |    32 |         7 |    48 |         7 |    48
    // Struct Padding (u128 only)          //        0 |    48 |         0 |    48 |         0 |    48
    // Free Padding to Cacheline           //       16 |    64 |        16 |    64 |        16 |    64
    // -- Cache Line --



    public:
    /// @name Lifecycle Management
    /// @{

    /// @brief Default constructor.
    Collatz() {}



    /**
    * @brief Constructor which calls `init()` right away.
    */
    Collatz(const T& initial_value) {
        init(initial_value);
    };



    /**
    * @brief Runs the sequence and builds the metadata, reusing the object if needed.
    * @param initial_value The initial value in the sequence.
    */
    void init(const T& initial_value) {
        if (initial_value < 0) {
            throw std::runtime_error("You cannot create a Collatz sequence with a value lower than 0.");
        }

        // Reset if necessary.
        if(_is_initialized) { reset(); }

        // Establish or clear metadata object.  Reset() already cleared it, if it existed.
        _is_initialized = true;
        _initial_value = initial_value;

        // Process the sequence and store any related metadata, if needed.  Otherwise, leave.
        try {
            for_each_sequence_step([&](const T& step) {
                _step_count++;
                if (step > _peak_value) {
                    _peak_value = step;
                }
                if (_hwm_index == 0 && step < _initial_value) {
                    _hwm_index = _step_count - 1;
                }
                return false;
            });
            // Decrement 1 to account for start position
            if (_step_count > 0) {
                _step_count--;
            }
        } catch(const CollatzSequenceOverflow& err) {
            _sequence_overflows = true;
            throw(err);
        }
    }



    /// @brief Reset members to make this act like a new() object.
    void reset() {
        _initial_value = 0;
        _peak_value = 0;
        _hwm_index = 0;
        _step_count = 0;
        _is_initialized = false;
        _sequence_overflows = false;
    }

    /// @}



    /// @name Accessors
    /// @{

    /// @brief Return the initial value set when created or when last `init()` was called.
    const T& get_initial_value() const { return _initial_value; }
    /// @brief Returns true if the sequence overflowed.
    bool has_overflowed() const { return _sequence_overflows; }
    /// @brief Returns true if the object has been initialized and is ready for use.
    bool is_initialized() const { return _is_initialized; }
    /// @brief Return the peak value found for the initial value sent.
    const T& get_peak_value() const { return _peak_value; }
    /// @brief Return the index of the sequence where the High-Water Mark was found for initial value sent.
    seq_size_t get_hwm_index() const { return _hwm_index; }
    /// @brief Return the step count of the sequence (stopping time) for initial value sent.
    seq_size_t get_step_count() const { return _step_count; }
    /// @brief Return the sequence as a vector of `T`.
    std::vector<T> get_sequence() const {
        if (! _is_initialized) {
            throw std::runtime_error("You cannot call get_sequence() when your Collatz object isn't initialized.");
        }
        std::vector<T> sequence;
        sequence.reserve(_step_count + 1);
        for_each_sequence_step([&](const T& value) {
            sequence.push_back(value);
            return false;
        });
        return sequence;
    }

    /// @}



    /// @brief Deeply scan the object and report its size.
    size_t deep_size() const {
        size_t total = sizeof(*this);
        return total;
    }



    /**
    * @brief Verify any number down to 1 (or a custom value) as fast as possible.
    *
    * Takes any initial value and verifies the sequence reaches 1 or the sentinel value requested.  This method handles overflow
    * detection and promotion by itself, and then calls the correctly typed implementation method.
    *
    * @note In the event a number is divergent (cyclic), this method will hang.
    * @param initial_value The starting value for a sequence.
    * @param sentinel_value The value to stop at.  Usually 1 or a High-Water Mark.
    * @return True if the sequence reached the sentinel value, hangs otherwise.
    */
    static inline bool st_verify(const T& initial_value, const T& sentinel_value = 1) {
        // Quick sanity check.  Sentinel value should be within the Collatz space (>=1) or it'll loop forever.
        // However, there are cases, such as a tree rooted at 0, where this might get tripped up.
        if (sentinel_value < 1) {
            throw std::out_of_range("You cannot send a sentinel value for st_verify below 1.  You sent: " + to_string_any(sentinel_value));
        }

        // Select the correct type to upgrade to, if needed for overflow.
        if constexpr(FixedWidthIntegral<T>) {
            if constexpr(sizeof(T) * 8 < 64) {
                // Type is less than 64 bits, just send it as a 64-bit.  Even uint32_t can't reach an overflow of 64-bit.
                // Max initial value of 64 bit is 12,327,829,502 which is greater than 2^32 which is 4,294,967,296.
                uint64_t safe_initial_value = initial_value;
                uint64_t safe_sentinel_value = sentinel_value;
                return st_call_verify_impl(safe_initial_value, safe_sentinel_value);
            } else if constexpr (sizeof(T) * 8 == 64) {
                // Dealing with T of exactly 64 bits.  If it'll overflow, use 128-bit.  Otherwise, pass through.
                if (initial_value > CollatzConstants::get_max_initial_value_by_bit<T>(64)) {
                    uint128_t safe_initial_value = initial_value;
                    uint128_t safe_sentinel_value = sentinel_value;
                    return st_call_verify_impl(safe_initial_value, safe_sentinel_value);
                }
            } else if constexpr (sizeof(T) * 8 == 128) {
                // Dealing with 128 bits.  If it'll overflow, use mpz_class.
                if (initial_value > CollatzConstants::get_max_initial_value_by_bit<T>(128)) {
                    // Extended will overflow, but mpz_class won't.  Assignment of 128 -> mpz_class uses helper.
                    static thread_local mpz_class safe_initial_value;
                    static thread_local mpz_class safe_sentinel_value;
                    Int128::uint128_to_mpz(initial_value, safe_initial_value);
                    Int128::uint128_to_mpz(sentinel_value, safe_sentinel_value);
                    return st_call_verify_impl(safe_initial_value, safe_sentinel_value);
                }
            }
        }

        // If no overflow detected, simply send the call as type T.
        return st_call_verify_impl(initial_value, sentinel_value);
    }



    /**
    * @brief A method to help handle sentinel value juggling for `st_verify()`.
    *
    * This method exists to help with some unrolling/micro-optimizing in the `st_verify_impl()` methods.  You probably shouldn't
    * call it manually.
    *
    * @note In the event a number is divergent (cyclic), this method will hang.
    * @tparam U Required type to avoid overflow, which may exceed T.
    * @param initial_value The starting value for a sequence.
    * @param sentinel_value The value to stop at.  Usually 1 or a High-Water Mark.
    * @return True if the sequence reached the sentinel value, hangs otherwise.
    */
    template<AnySupportedIntegral U>
    static inline bool st_call_verify_impl(const U& initial_value, const U& sentinel_value) {
        if (sentinel_value == 1) {
            return Collatz<U>::st_verify_impl(initial_value);
        } else {
            return Collatz<U>::st_verify_impl(initial_value, sentinel_value);
        }
    }



    /**
    * @brief Verification implementation, which assumes down to `1`.
    * @note This version saves a few clock cycles compared to the dynamic sentinel value version of this method.
    * @param initial_value The starting value for a sequence.
    * @return True if the sequence reached the sentinel value, hangs otherwise.
    */
    static inline bool st_verify_impl(const T& initial_value) {
        // Use a copy in a TLS to avoid alloc on GMP type, and to avoid mangling caller's data.
        static thread_local T tls_initial_value;
        tls_initial_value = initial_value;
        while (tls_initial_value > 1) {
            if constexpr(FixedWidthIntegral<T>) {
                // The key optimizations are shifting by CTZ and using modulo instead of bitwise check.
                if (tls_initial_value % 2 == 1) {
                    // Odd
                    tls_initial_value = (tls_initial_value << 1) + tls_initial_value + 1;
                }
                // Always even at this point, so shift by CTZ.
                tls_initial_value >>= Bit::count_trailing_zeros(tls_initial_value);
            } else if constexpr(GMPIntegral<T>) {
                // GMP optimizations are to use bitwise check, shift by CTZ, and leverage an affine map for consecutive ones.
                static thread_local CollatzAffineMap<T> af_map;
                static thread_local T tls_out;
                if ((tls_initial_value & 1) == 1) {
                    // Odd
                    af_map.reset();
                    size_t trailing_ones = Bit::count_trailing_ones(tls_initial_value);
                    for (size_t i = 0; i < trailing_ones; i++) {
                        af_map.apply_F();
                    }
                    af_map.calculate(tls_initial_value, tls_out);
                    tls_initial_value = tls_out;
                }
                // Always even at this point, so shift by CTZ.
                tls_initial_value >>= Bit::count_trailing_zeros(tls_initial_value);
            }
        }

        return true;
    }



    /**
    * @brief Verification implementation, which tests down to `sentinel_value`.
    * @param initial_value The starting value for a sequence.
    * @param sentinel_value The value to stop at.  Usually 1 or a High-Water Mark.
    * @return True if the sequence reached the sentinel value, hangs otherwise.
    */
    static inline bool st_verify_impl(const T& initial_value, const T& sentinel_value) {
        // Use a copy in a TLS to avoid alloc on GMP type, and to avoid mangling caller's data.
        static thread_local T tls_initial_value;
        tls_initial_value = initial_value;
        while (tls_initial_value > sentinel_value) {
            if constexpr(FixedWidthIntegral<T>) {
                // The key optimizations are shifting by CTZ and using modulo instead of bitwise check.
                if (tls_initial_value % 2 == 1) {
                    // Odd
                    tls_initial_value = (tls_initial_value << 1) + tls_initial_value + 1;
                }
                // Always even at this point, so shift by CTZ.
                tls_initial_value >>= Bit::count_trailing_zeros(tls_initial_value);
            } else {
                // GMP optimizations are to use bitwise check, shift by CTZ, and leverage an affine map for consecutive ones.
                static thread_local CollatzAffineMap<T> af_map;
                static thread_local T tls_out;
                if ((tls_initial_value & 1) == 1) {
                    // Odd
                    af_map.reset();
                    size_t trailing_ones = Bit::count_trailing_ones(tls_initial_value);
                    for (size_t i = 0; i < trailing_ones; i++) {
                        af_map.apply_F();
                    }
                    af_map.calculate(tls_initial_value, tls_out);
                    tls_initial_value = tls_out;
                }
                // Always even at this point, so shift by CTZ.
                tls_initial_value >>= Bit::count_trailing_zeros(tls_initial_value);
            }
        }

        return true;
    }



    /**
    * @brief Run through the sequence with a callback each step, including the initial value.
    * @tparam Func A callback function type.
    * @param initial_value The first value of a sequence to process.  Will be returned as the first step.
    * @param callback Caller's callback.  Must be callable as `const T&` to prevent alloc on GMP path.  Must return true to stop
    * early, false otherwise.
    */
    template<typename Func>
    static void st_for_each_sequence_step(const T& initial_value, Func&& callback) {
        // Do not allow non-ref callbacks.  Otherwise it can make GMP over and over.
        static_assert(std::is_invocable_v<Func, const T&>, "Callback must be callable with (const T&)");

        // Zero is a special case, mostly for BinaryTree building a root.
        if (initial_value == 0) { return; }

        if constexpr(FixedWidthIntegral<T>) {
            // Fixed integrals can use intrinsic arithmetic operators for "free", but can overflow.
            T current_value = initial_value;
            while (current_value != 1) {
                bool stop = callback(current_value);
                if (stop) { return; }
                if ((current_value & 1) == 0) {
                    current_value >>= 1;
                } else {
                    if (current_value > CollatzConstants::get_max_3xp1<T>()) {
                        throw CollatzSequenceOverflow("Overflow when building for_each_sequence_step().");
                    }
                    current_value = (current_value << 1) + current_value + 1;
                }
            }
            // Since the while-loop exits prematurely at 1, it needs one more callback before ending.
            callback(current_value);
        } else if constexpr(GMPIntegral<T>) {
            // GMP integers will alloc() with certain arithmetic operators, but can't overflow.
            thread_local T current_value;
            current_value = initial_value;
            while (mpz_cmp_ui(current_value.get_mpz_t(), 1) != 0) {
                bool stop = callback(current_value);
                if (stop) { return; }
                if (mpz_even_p(current_value.get_mpz_t())) {
                    mpz_tdiv_q_2exp(current_value.get_mpz_t(), current_value.get_mpz_t(), 1);  // current_step >> 1   ==>  current_step /= 2
                } else {
                    mpz_mul(current_value.get_mpz_t(), current_value.get_mpz_t(), CollatzConstants::MPZ_THREE.get_mpz_t());  // current_step *= 3
                    mpz_add(current_value.get_mpz_t(), current_value.get_mpz_t(), CollatzConstants::MPZ_ONE.get_mpz_t());    // current_step += 1
                }
            }
            // Since the while-loop exits prematurely at 1, it needs one more callback before ending.
            callback(current_value);
        }
    }



    /// @brief Run through all the steps of a sequence.  Uses `st_for_each_sequence_step()` under the hood.
    template<typename Func>
    void for_each_sequence_step(Func&& callback) const {
        Collatz<T>::st_for_each_sequence_step(_initial_value, std::forward<Func>(callback));
    }



    /**
    * @brief Returns the number of steps (stopping time) for any initial value.
    * @param initial_value The first value of a sequence to calculate.
    * @return Number of steps (stopping time) to reach 1.
    */
    static seq_size_t st_get_step_count(const T& initial_value) {
        size_t right_shifts = 0;
        seq_size_t steps = 0;

        if constexpr(FixedWidthIntegral<T>) {
            // Native types are fast as-is.  Affine compression doesn't help, except bit-shifting CTZ.
            T tmp = initial_value;
            // Check for overflow here, once, instead of over and over.
            if (initial_value > CollatzConstants::get_max_initial_value_by_bit<T>(std::numeric_limits<T>::digits)) {
                throw CollatzSequenceOverflow("Overflow when building st_get_step_count().");
            }
            while (tmp > 1) {
                // Handle odd.
                if ((tmp & 1) == 1) {
                    tmp = (tmp << 1) + tmp + 1;
                    steps++;
                }
                // Always even at this point.  Shift zeros out.
                right_shifts = Bit::count_trailing_zeros(tmp);
                tmp >>= right_shifts;
                steps += right_shifts;
            }
        } else if constexpr(GMPIntegral<T>) {
            // GMP types are significantly faster using an Affine map for CTZ and CTO, ergo it's worth the overhead.
            static thread_local T tmp;
            tmp = initial_value;
            size_t trailing_ones = 0;
            constexpr size_t limit = Exponents::POW3_MPZ_ELEMENT_COUNT - 1;
            // See collatz_compression.cpp tests for details on how this works and why.
            while (tmp > 1) {
                // Handle odd.
                if ((tmp & 1) == 1) {
                    trailing_ones = Bit::count_trailing_ones(tmp);
                    steps += (2 * trailing_ones);
                    while (trailing_ones > limit) {
                        tmp = ((Exponents::POW3_MPZ[limit] * (tmp + 1)) >> limit) - 1;
                        trailing_ones -= limit;
                    }
                    if (trailing_ones > 0) {
                        tmp = ((Exponents::POW3_MPZ[trailing_ones] * (tmp + 1)) >> trailing_ones) - 1;
                    }
                }
                // Always even at this point.  Shift zeros out.
                right_shifts = Bit::count_trailing_zeros(tmp);
                tmp >>= right_shifts;
                steps += right_shifts;
            }
        }
        return steps;
    }



    /**
    * @brief Finds the peak value of a sequence, optionally stopping at High-Water Mark.
    * @param initial_value The first value of a sequence to calculate.
    * @param out_peak Caller's memory to store peak value.
    * @param stop_at_hwm Flag to stop at High-Water Mark when testing peaks, mostly for the peak-by-bit program.
    */
    static inline void st_get_peak(const T& initial_value, T& out_peak, bool stop_at_hwm = false) {
        size_t right_shifts = 0;
        out_peak = initial_value;

        if constexpr(FixedWidthIntegral<T>) {
            // Native types are fast as-is.  Affine compression doesn't help, except bit-shifting CTZ.
            T tmp = initial_value;
            T bailout_value = (stop_at_hwm && initial_value > 1) ? (T)initial_value - 1 : T(1);
            while (tmp > bailout_value) {
                // Handle odd.
                if ((tmp & 1) == 1) {
                    if (tmp > CollatzConstants::get_max_3xp1<T>()) {
                        throw std::out_of_range("Cannot process initial_value " + to_string_any(initial_value) + " any further in st_get_peak.");
                    }
                    tmp = (tmp << 1) + tmp + 1;
                    if (tmp > out_peak) {
                        out_peak = tmp;
                    }
                }
                // Always even at this point.  Shift zeros out.  Can't affect peak.
                right_shifts = Bit::count_trailing_zeros(tmp);
                tmp >>= right_shifts;
            }
        } else if constexpr(GMPIntegral<T>) {
            // GMP types are significantly faster using an Affine map for CTZ and CTO, ergo it's worth the overhead.
            static thread_local T tmp;
            tmp = initial_value;
            static thread_local T tmp_x_2;
            static thread_local T bailout_value;
            bailout_value = 1;
            if (stop_at_hwm && initial_value > 1) {
                bailout_value = initial_value;
                mpz_sub_ui(bailout_value.get_mpz_t(), bailout_value.get_mpz_t(), 1);
            }
            size_t trailing_ones = 0;
            constexpr size_t limit = Exponents::POW3_MPZ_ELEMENT_COUNT - 1;
            // See collatz_compression.cpp tests for details on how this works and why.
            while (tmp > bailout_value) {
                // Handle odd.
                if (mpz_odd_p(tmp.get_mpz_t())) {
                    trailing_ones = Bit::count_trailing_ones(tmp);
                    while (trailing_ones > limit) {
                        mpz_add_ui(tmp.get_mpz_t(), tmp.get_mpz_t(), 1);
                        mpz_mul(tmp.get_mpz_t(), tmp.get_mpz_t(), Exponents::POW3_MPZ[limit].get_mpz_t());
                        mpz_tdiv_q_2exp(tmp.get_mpz_t(), tmp.get_mpz_t(), limit);
                        mpz_sub_ui(tmp.get_mpz_t(), tmp.get_mpz_t(), 1);
                        trailing_ones -= limit;
                    }
                    if (trailing_ones > 0) {
                        mpz_add_ui(tmp.get_mpz_t(), tmp.get_mpz_t(), 1);
                        mpz_mul(tmp.get_mpz_t(), tmp.get_mpz_t(), Exponents::POW3_MPZ[trailing_ones].get_mpz_t());
                        mpz_tdiv_q_2exp(tmp.get_mpz_t(), tmp.get_mpz_t(), trailing_ones);
                        mpz_sub_ui(tmp.get_mpz_t(), tmp.get_mpz_t(), 1);
                    }
                    // Now check peak.  However!  It applied an accelerated F(x):  (3x + 1) / 2
                    // Peak was actually tmp * 2.
                    mpz_mul_2exp(tmp_x_2.get_mpz_t(), tmp.get_mpz_t(), 1);
                    if (mpz_cmp(tmp_x_2.get_mpz_t(), out_peak.get_mpz_t()) > 0) {
                        out_peak = tmp_x_2;
                    }
                }
                // Always even at this point.  Shift zeros out.
                right_shifts = Bit::count_trailing_zeros(tmp);
                tmp >>= right_shifts;
            }
        }
    }



    /**
    * @brief Generate an FG chain on-the-fly and return the link (F or G) to a callback.
    * @tparam Func Caller's function type.  Must be: `bool(bool)`
    * @param initial_value The first value of the sequence.
    * @param callback The method to invoke with each F or G step.  When F, true.  When G, false.  Caller may return false to stop
    * execution at any time.
    */
    template<typename Func>
    static void st_for_each_fg_chain_link(const T& initial_value, Func&& callback) {
        // Do not allow non-ref callbacks.  Otherwise it makes GMP over and over.
        static_assert(std::is_invocable_r_v<bool, Func, bool>, "Callback must be callable as bool(bool)");

        // Zero is a special case, mostly for BinaryTree building a root.
        if (initial_value == 0) { return; }

        // Call our basic Collatz iterator.
        bool is_F = false;
        Collatz<T>::st_for_each_sequence_step(initial_value, [&](const T& current_value) {
            // When the previous step was F, skip the next step which is always the latter part of accerlated F(x).
            if (is_F) {
                // Reset it so it doesn't loop.
                is_F = false;
                return false;
            } else {
                if constexpr(FixedWidthIntegral<T>) {
                    is_F = (current_value & 1) == 1;
                } else if constexpr(GMPIntegral<T>) {
                    is_F = mpz_odd_p(current_value.get_mpz_t());
                }
                bool stop = callback(is_F);
                return stop;
            }
        });
    }



    /**
    * @brief Steps through a Collatz sequence using F and G steps instead of normal ones.
    * @tparam Func Caller's function type.  Must be: `const T&`
    * @param initial_value The first value of the sequence.
    * @param callback The method to invoke with each F or G step's value.  Caller may return false to stop execution at any time.
    */
    template<typename Func>
    static void st_for_each_fg_step(const T& initial_value, Func&& callback) {
        // Do not allow non-ref callbacks.  Otherwise it makes GMP over and over.
        static_assert(std::is_invocable_v<Func, const T&>, "Callback must be callable with (const T&)");

        // Zero is a special case, mostly for BinaryTree building a root.
        if (initial_value == 0) { return; }

        // Call our basic Collatz iterator.
        bool was_F = false;
        Collatz<T>::st_for_each_sequence_step(initial_value, [&](const T& current_value) {
            // When the previous step was F, skip the next step which is always the latter part of accerlated F(x).
            if (was_F) {
                // Reset it so it doesn't loop.
                was_F = false;
                return false;
            } else {
                if constexpr(FixedWidthIntegral<T>) {
                    was_F = (current_value & 1) == 1;
                } else if constexpr(GMPIntegral<T>) {
                    was_F = mpz_odd_p(current_value.get_mpz_t());
                }
                bool stop = callback(current_value);
                return stop;
            }
        });
    }



    /**
    * @brief Generate the F-G chain as a string for a sequence with some initial value.
    * @param initial_value The first value of the sequence to calculate.
    * @param max_chars The maximum number of F and G characters (steps) to accumulate before quitting prematurely.
    * @return The string representation of the full FG chain.
    */
    static std::string st_get_fg_chain_string(const T& initial_value, seq_size_t max_chars = std::numeric_limits<seq_size_t>::max()) {
        std::string result;
        seq_size_t count = 0;
        Collatz<T>::st_for_each_fg_chain_link(initial_value, [&](bool is_F) {
            result += (is_F ? 'F' : 'G');
            count++;
            return count >= max_chars;
        });
        return result;
    }



    /// @brief Generate the F-G chain as a string for a sequence with some initial value.  Uses `st_get_fg_chain_string()`.
    std::string get_fg_chain_string(seq_size_t max_chars = std::numeric_limits<seq_size_t>::max()) const {
        return Collatz<T>::st_get_fg_chain_string(_initial_value, max_chars);
    }



    /**
    * @brief Generate the O-E (odd-even) chain as a string for a sequence with some initial value.
    * @param initial_value The first value of the sequence to calculate.
    * @param max_chars The maximum number of O and E characters (steps) to accumulate before quitting prematurely.
    * @return The string representation of the full OE chain.
    */
    static std::string st_get_oe_pattern_string(const T& initial_value, seq_size_t max_chars = std::numeric_limits<seq_size_t>::max()) {
        std::string fg_pattern = Collatz<T>::st_get_fg_chain_string(initial_value, max_chars);
        return st_fg_to_oe(fg_pattern, max_chars);
    }



    /// @brief Generate the O-E chain as a string for a sequence with some initial value.  Uses `st_get_oe_pattern_string()`.
    std::string get_oe_pattern_string(seq_size_t max_chars = std::numeric_limits<seq_size_t>::max()) {
        return Collatz<T>::st_get_oe_pattern_string(_initial_value, max_chars);
    }



    /**
    * @brief Convert an FG chain (string) to an OE pattern/chain (string).
    * @param fg_pattern The FG chain to convert.
    * @param max_oe_chars The maximum number of O and E characters (steps) to accumulate before quitting prematurely.
    * @param strip_last_e Remove the last E.  This is necessary when going down to 1, for example.  Because 'F' => OE, which would
    * send the value back to 4 and make the OE chain wrong.  If stopping at a value other than 1, this might need to be false.
    * @return A string representation of the OE chain.
    */
    static std::string st_fg_to_oe(
        const std::string& fg_pattern
        , size_t max_oe_chars = std::numeric_limits<size_t>::max()
        , bool strip_last_e = true
    ) {
        std::string oe_string;
        for (const char& c : fg_pattern) {
            if (c == 'F') {
                oe_string += "OE";
            } else {
                oe_string += "E";
            }
            if (oe_string.size() > max_oe_chars) {
                break;
            }
        }

        // If this didn't hit the max_chars, remove the last E.  Because all sequences end in 1 (odd) and therefore get an "F",
        // which traslates to "OE", but that last "E" is 1 going to 4 (1 * 3 + 1).
        // If the caller forbids this, skip it.
        if (strip_last_e && oe_string.size() < max_oe_chars && oe_string.size() > 0) {
            oe_string.pop_back();
        }

        // If it's over the max_chars requested, trim.
        if (oe_string.size() > max_oe_chars) {
            oe_string.resize(max_oe_chars);
        }

        return oe_string;
    }
};
