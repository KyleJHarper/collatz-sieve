#pragma once

#include <gmp.h>
#include <gmpxx.h>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include <string>
#include <stdint.h>
#include "concepts.hpp"
#include "gmp_helpers.hpp"
#include "count_trailing_helpers.hpp"
#include "exponents.hpp"
#include "collatz_affine_map.hpp"
#include "collatz_constants.hpp"




/**
* @exception CollatzSequenceOverflow
* @brief Custom overflow exception for Collatz sequences.
*/
class CollatzSequenceOverflow : public std::runtime_error {
    public:
    explicit CollatzSequenceOverflow(const std::string& msg) : std::runtime_error(msg) {}
};




//
// Collatz Metadata
//
// Metadata bloats the Collatz class, so it's offloaded here and disabled by default.  Collatz() retains a pointer to
// a CollatzMetadata object, keeping its size fixed regardless of how much metadata we create in the future.  The logic
// is: if a caller wants the full metadata, they're already willing to pay the cost of having metadata, so an extra 8
// bytes in a pointer and a 8-16 bytes for the CollatzMetadata object (alloc overhead) doesn't matter as much.
//
// This design decision favors callers who don't want Metadata: they get reduced memory footprint at the expense of
// callers who want metadata needing pointer dereferencing.
//
// Summary of the time-memory trade-off:
//   time  -> callers who don't want CollatzMetadata will calculated it each time a getter for metadata is called.
//   memory-> callers who do want CollatzMetadata have it on-demand, computed when Collatz().init() is called.
//
template<AnySupportedIntegral T>
class CollatzMetadata {
    private:
    // None, just make them public.



    public:
    // Set to zero!  GMP types are not default initialized to 0.
    T peak_value = 0;
    seq_size_t hwm_index = 0;
    seq_size_t step_count = 0;
    CollatzMetadata() {}



    //
    // Reset
    // Default-initialize manually.
    //
    void reset() {
        peak_value = 0;
        hwm_index = 0;
        step_count = 0;
    }



    //
    // Object Size
    // Deeply scan the object, including pool and buffers.
    //
    size_t deep_size() const {
        size_t total = sizeof(*this);
        if constexpr(GMPIntegral<T>) {
            total += gmp_deep_sizeof(peak_value);
        }
        return total;
    }
};




//
// Collatz
//
// Basic Collatz sequence object with minimal data to keep it tight.
// see: CollatzMetadata for details about time-memory trade-offs with _enable_metadata.
//
template<AnySupportedIntegral T>
class Collatz {
    private:
    // Memory packing and alignment matter!  Keep this class LIGHT unless the caller wants metadata.
    // All data must fit within one cache line.
    //                                           uint64_t | total | uint128_t | total | mpz_class | total
    T _initial_value;                         //        8 |     8 |        16 |    16 |        16 |    16
    bool _is_initialized : 1;                 //      1:1 |     9 |       1:1 |    17 |       1:1 |    17
    bool _track_sequence : 1;                 //      1:2 |     9 |       1:2 |    17 |       1:2 |    17
    bool _track_metadata : 1;                 //      1:3 |     9 |       1:3 |    17 |       1:3 |    17
    bool _sequence_overflow : 1;              //      1:4 |     9 |       1:4 |    17 |       1:4 |    17  (4 bits padding)
    // Alignment Padding                      //        7 |    16 |         7 |    24 |         7 |    24
    std::vector<T> _sequence;                 //       24 |    40 |        24 |    48 |        24 |    48
    CollatzMetadata<T>* _metadata = nullptr;  //        8 |    48 |         8 |    56 |         8 |    56
    // Struct Alignment Padding (u128 only)   //        0 |    48 |         8 |    64 |         0 |    56
    // Free Padding to Cacheline              //       16 |    64 |         0 |    64 |         8 |    64
    // -- Cache Line --



    public:
    // Reusable messages.
    static inline std::string E_NO_SEQUENCE_TRACKING = "You disabled sequence tracking when you created this object.";
    static inline std::string E_NO_METADATA_TRACKING = "You disabled metadata when you created this object.";


    //
    // Constructors.  Offload to init() so objects can be reused.
    //
    Collatz() {}
    Collatz(const T& initial_value, bool track_sequence = false, bool track_metadata = false) {
        init(initial_value, track_sequence, track_metadata);
    };



    //
    // Destructor
    //
    ~Collatz() {
        release_metadata();
    }



    //
    // Cout Friend
    // Treat the initial value as the <<() output.
    //
    friend std::ostream& operator<<(std::ostream &os, const Collatz<T>& m) {
        return os << m._initial_value;
    }



    //
    // Initialize
    // Builds the object, reusing it if necessary.
    //
    void init(const T& initial_value, bool track_sequence = false, bool track_metadata = false) {
        if (initial_value < 0) {
            throw std::runtime_error("You cannot create a Collatz sequence with a value lower than 0.");
        }

        // Reset if necessary.
        if(_is_initialized) { reset(); }

        // Establish or clear metadata object.  Reset() already cleared it, if it existed.
        if (_metadata == nullptr && track_metadata) { _metadata = new CollatzMetadata<T>(); }
        if (_metadata != nullptr && ! track_metadata) { release_metadata(); }
        _track_sequence = track_sequence;
        _track_metadata = track_metadata;
        _is_initialized = true;
        _initial_value = initial_value;

        // Process the sequence and store any related metadata, if needed.  Otherwise, leave.
        if (_track_sequence || _track_metadata) {
            try {
                for_each_sequence_step([&](const T& step) {
                    if (_track_sequence) { _sequence.push_back(step); }
                    if (_track_metadata) {
                        _metadata->step_count++;
                        if (step > _metadata->peak_value) {
                            _metadata->peak_value = step;
                        }
                        if (_metadata->hwm_index == 0 && step < _initial_value) {
                            _metadata->hwm_index = _metadata->step_count - 1;
                        }
                    }
                    return false;
                });
                // Decrement 1 to account for start position
                if (_track_metadata && _metadata->step_count > 0) {
                    _metadata->step_count--;
                }
            } catch(const CollatzSequenceOverflow& err) {
                _sequence_overflow = true;
                throw(err);
            }
            if (_track_sequence) {
                _sequence.shrink_to_fit();
            }
        }
    }



    //
    // Reset Object
    // Reset members to make this act like a new() object.
    //
    void reset() {
        _sequence.clear();
        _sequence.shrink_to_fit();
        _sequence_overflow = false;
        _track_sequence = false;
        _track_metadata = false;
        if (_metadata != nullptr) { _metadata->reset(); }
    }



    //
    // Release Metadata
    // Let callers decide when they're done with metadata.
    //
    void release_metadata() {
        if (_metadata == nullptr) { return; }
        delete _metadata;
        _metadata = nullptr;
        _track_metadata = false;
    }



    //
    // Getters
    //
    const T& get_initial_value() const { return _initial_value; }
    bool get_is_overflowed() const { return _sequence_overflow; }
    bool get_is_initialized() const { return _is_initialized; }
    bool get_track_sequence() const { return _track_sequence; }
    bool get_track_metadata() const { return _track_metadata; }
    const CollatzMetadata<T>* get_metadata() const { return _metadata; }
    //
    // Sequence and metadata accessors.
    const std::vector<T>& get_sequence() const {
        if(!_track_sequence) {
            throw std::logic_error(E_NO_SEQUENCE_TRACKING);
        }
        return _sequence;
    };
    const T& get_peak_value() const {
        if (! _track_metadata) {
            throw std::logic_error(E_NO_METADATA_TRACKING);
        }
        return _metadata->peak_value;
    }
    size_t get_hwm_index() const {
        if (! _track_metadata) {
            throw std::logic_error(E_NO_METADATA_TRACKING);
        }
        return static_cast<size_t>(_metadata->hwm_index);
    }
    size_t get_step_count() const {
        if (! _track_metadata) {
            throw std::logic_error(E_NO_METADATA_TRACKING);
        }
        return static_cast<size_t>(_metadata->step_count);
    }



    //
    // Sequence String
    // Build a string version of all steps in a single, comma-separated string.
    //
    std::string get_sequence_string() {
        std::string rv;
        rv.append(to_string_any(_initial_value));

        for(size_t i=1; i<_sequence.size(); i++) {
            rv.append(", ");
            rv.append(to_string_any(_sequence[i]));
        }

        return rv;
    }



    //
    // Object Size
    // Deeply scan the object, including pool and buffers.
    //
    size_t deep_size() const {
        size_t total = sizeof(*this);

        if constexpr(BuiltinIntegral<T>) {
            total += sizeof(T) * _sequence.capacity();
        } else if constexpr(GMPIntegral<T>) {
            total += sizeof(mpz_class) * _sequence.capacity();
            for (const auto& val : _sequence) {
                total += gmp_deep_sizeof(val);
            }
        }

        if (_metadata != nullptr) {
            total += _metadata->deep_size();
        }

        return total;
    }



    //
    // Verify (Full and Sentinel)
    // Verify any number down to 1 (or a custom value) as fast as possible.  Returns true (verified) or loops forever (divergence
    // found).  Handles overflow detection and promotion by itself, then calls the _impl method associated.
    //
    // See collatz_compression for choices concerning optimizations.
    //
    //
    // Main verify method.
    static inline bool st_verify(const T& initial_value, const T& sentinel_value = 1) {
        // Quick sanity check.  Sentinel value should be within the Collatz space (>=1) or we'll loop forever.
        // However, there are cases, such as a tree rooted at 0, where this might get tripped up.
        if (sentinel_value < 1) {
            throw std::out_of_range("You cannot send a sentinel value for st_verify below 1.  You sent: " + to_string_any(sentinel_value));
        }

        // Select the correct type to upgrade to, if needed for overflow.
        if constexpr(NativeIntegral<T>) {
            // If the type is less than 64 bits, just send it as a 64-bit.  Even uint32_t can't reach an overflow of 64-bit.
            // Max initial value of 64 bit is 12,327,829,502 which is greater than 2^32 which is 4,294,967,296.
            if constexpr(std::numeric_limits<T>::digits < 64) {
                uint64_t safe_initial_value = initial_value;
                uint64_t safe_sentinel_value = sentinel_value;
                return st_call_verify_impl(safe_initial_value, safe_sentinel_value);
            }
            // Dealing with T of exactly 64 bits.  If it'll overflow, use 128-bit.
            if (initial_value > CollatzConstants::get_max_initial_value_by_bit<T>(64)) {
                // Native will overflow, but 128-bit won't.  Assignment of 64->128 is safe.
                uint128_t safe_initial_value = initial_value;
                uint128_t safe_sentinel_value = sentinel_value;
                return st_call_verify_impl(safe_initial_value, safe_sentinel_value);
            }
        } else if constexpr(ExtendedIntegral<T>) {
            // Dealing with 128 bits.  If it'll overflow, use mpz_class.
            if (initial_value > CollatzConstants::get_max_initial_value_by_bit<T>(128)) {
                // Extended will overflow, but mpz_class won't.  Assignment of 128 -> mpz_class uses helper.
                static thread_local mpz_class safe_initial_value;
                static thread_local mpz_class safe_sentinel_value;
                uint128_to_mpz(initial_value, safe_initial_value);
                uint128_to_mpz(sentinel_value, safe_sentinel_value);
                return st_call_verify_impl(safe_initial_value, safe_sentinel_value);
            }
        }

        // If we didn't detect an overflow above, we can simply send the call as type T.
        return st_call_verify_impl(initial_value, sentinel_value);
    }
    //
    // A caller to help handle sentinel value juggling.
    template<AnySupportedIntegral U>
    static inline bool st_call_verify_impl(const U& initial_value, const U& sentinel_value) {
        if (sentinel_value == 1) {
            return Collatz<U>::st_verify_impl(initial_value);
        } else {
            return Collatz<U>::st_verify_impl(initial_value, sentinel_value);
        }
    }
    //
    // The actual implementation of verification.  This assumes "down to 1".  An alternate is provided below.  This saves a few
    // clock cycles.
    static inline bool st_verify_impl(const T& initial_value) {
        // Use a copy in a TLS to avoid alloc on GMP type, and to avoid mangling caller's data.
        static thread_local T tls_initial_value;
        tls_initial_value = initial_value;
        while (tls_initial_value > 1) {
            if constexpr(BuiltinIntegral<T>) {
                // The key optimizations are shifting by CTZ and using modulo instead of bitwise check.
                if (tls_initial_value % 2 == 1) {
                    // Odd
                    tls_initial_value = (tls_initial_value << 1) + tls_initial_value + 1;
                }
                // Always even at this point, so shift by CTZ.
                tls_initial_value >>= count_trailing_zeros(tls_initial_value);
            } else {
                // GMP optimizations are to use bitwise check, shift by CTZ, and leverage an affine map for consecutive ones.
                static thread_local CollatzAffineMap<T> af_map;
                static thread_local T tls_out;
                if ((tls_initial_value & 1) == 1) {
                    // Odd
                    af_map.reset();
                    size_t trailing_ones = count_trailing_ones(tls_initial_value);
                    for (size_t i = 0; i < trailing_ones; i++) {
                        af_map.apply_F();
                    }
                    af_map.calculate(tls_initial_value, tls_out);
                    tls_initial_value = tls_out;
                }
                // Always even at this point, so shift by CTZ.
                tls_initial_value >>= count_trailing_zeros(tls_initial_value);
            }
        }

        return true;
    }
    //
    // Alternate to allow a custom sentinel value, such as a high-water mark.
    static inline bool st_verify_impl(const T& initial_value, const T& sentinel_value) {
        // Use a copy in a TLS to avoid alloc on GMP type, and to avoid mangling caller's data.
        static thread_local T tls_initial_value;
        tls_initial_value = initial_value;
        while (tls_initial_value > sentinel_value) {
            if constexpr(BuiltinIntegral<T>) {
                // The key optimizations are shifting by CTZ and using modulo instead of bitwise check.
                if (tls_initial_value % 2 == 1) {
                    // Odd
                    tls_initial_value = (tls_initial_value << 1) + tls_initial_value + 1;
                }
                // Always even at this point, so shift by CTZ.
                tls_initial_value >>= count_trailing_zeros(tls_initial_value);
            } else {
                // GMP optimizations are to use bitwise check, shift by CTZ, and leverage an affine map for consecutive ones.
                static thread_local CollatzAffineMap<T> af_map;
                static thread_local T tls_out;
                if ((tls_initial_value & 1) == 1) {
                    // Odd
                    af_map.reset();
                    size_t trailing_ones = count_trailing_ones(tls_initial_value);
                    for (size_t i = 0; i < trailing_ones; i++) {
                        af_map.apply_F();
                    }
                    af_map.calculate(tls_initial_value, tls_out);
                    tls_initial_value = tls_out;
                }
                // Always even at this point, so shift by CTZ.
                tls_initial_value >>= count_trailing_zeros(tls_initial_value);
            }
        }

        return true;
    }



    //
    // For-Each Step
    // Run through the sequence with a callback each step, including the initial value.
    //
    // Caller MUST return true or false to stop or continue.
    //
    template<typename Func>
    static void for_each_sequence_step(const T& initial_value, Func&& callback) {
        // Do not allow non-ref callbacks.  Otherwise we make GMP over and over.
        static_assert(std::is_invocable_v<Func, const T&>, "Callback must be callable with (const T&)");

        // Zero is a special case, mostly for BinaryTree building a root.
        if (initial_value == 0) { return; }

        if constexpr(BuiltinIntegral<T>) {
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
            // Since the while-loop exits prematurely at 1, we need one more callback before we end.
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
            // Since the while-loop exits prematurely at 1, we need one more callback before we end.
            callback(current_value);
        }
    }
    //
    // Wrapper for the instance implementation.
    template<typename Func>
    void for_each_sequence_step(Func&& callback) const {
        // Do not allow non-ref callbacks.  Otherwise we make GMP over and over.
        static_assert(std::is_invocable_v<Func, const T&>, "Callback must be callable with (const T&)");

        // Sequence exists?  Use it directly.
        if (_sequence.size() > 0) {
            for (const T& current_step : _sequence) {
                bool stop = callback(current_step);
                if (stop) { return; }
            }
            return;
        }

        // Sequence didn't exist.  Calculate it on-the-fly via the static method.
        Collatz<T>::for_each_sequence_step(_initial_value, std::forward<Func>(callback));
    }



    //
    // Step Count
    // Returns the step count for any initial value.  Recall it's sequence size - 1.
    //
    static size_t st_get_step_count(const T& initial_value) {
        size_t steps = 0;
        for_each_sequence_step(initial_value, [&](const T& step) {
            steps++;
            return step < 0;  // We need to return false anyway, might as well get rid of a compiler warning.
        });
        return steps - 1;
    }
    //
    //
    // Step Count (Fast Variant)
    // Skips the for_each_sequence_step() iterator to minimize overhead.  See collatz_compression.cpp for details.
    //
    static size_t st_get_step_count_fast(const T& initial_value) {
        size_t right_shifts = 0;
        size_t steps = 0;

        if constexpr(BuiltinIntegral<T>) {
            // Native types are fast as-is.  Affine compression doesn't help, except bit-shifting CTZ.
            T tmp = initial_value;
            // Check for overflow here, once, instead of over and over.
            if (initial_value > CollatzConstants::get_max_initial_value_by_bit<T>(std::numeric_limits<T>::digits)) {
                throw CollatzSequenceOverflow("Overflow when building st_get_step_count_fast().");
            }
            while (tmp > 1) {
                // Handle odd.
                if ((tmp & 1) == 1) {
                    tmp = (tmp << 1) + tmp + 1;
                    steps++;
                }
                // Always even at this point.  Shift zeros out.
                right_shifts = count_trailing_zeros(tmp);
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
                    trailing_ones = count_trailing_ones(tmp);
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
                right_shifts = count_trailing_zeros(tmp);
                tmp >>= right_shifts;
                steps += right_shifts;
            }
        } else {
            throw std::runtime_error("Cannot determine data type for st_get_step_count_fast().");
        }
        return steps;
    }


    //
    // Get Peak
    // Finds the peak of a sequence.  Skips the for_each_sequence_step() iterator to minimize overhead.  See
    // collatz_compression.cpp for details.
    //
    // You may optionally stop at high-water mark (mainly for peak_by_bit program).
    //
    static inline void st_get_peak_fast(const T& initial_value, T& out_peak, bool stop_at_hwm = false) {
        size_t right_shifts = 0;
        out_peak = initial_value;

        if constexpr(BuiltinIntegral<T>) {
            // Native types are fast as-is.  Affine compression doesn't help, except bit-shifting CTZ.
            T tmp = initial_value;
            T bailout_value = (stop_at_hwm && initial_value > 1) ? (T)initial_value - 1 : T(1);
            while (tmp > bailout_value) {
                // Handle odd.
                if ((tmp & 1) == 1) {
                    if (tmp > CollatzConstants::get_max_3xp1<T>()) {
                        throw std::out_of_range("Cannot process initial_value " + to_string_any(initial_value) + " any further in st_get_peak_fast.");
                    }
                    tmp = (tmp << 1) + tmp + 1;
                    if (tmp > out_peak) {
                        out_peak = tmp;
                    }
                }
                // Always even at this point.  Shift zeros out.  Can't affect peak.
                right_shifts = count_trailing_zeros(tmp);
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
                    trailing_ones = count_trailing_ones(tmp);
                    while (trailing_ones > limit) {
                        // tmp = ((CollatzConstants::POW3_MPZ[limit] * (tmp + 1)) >> limit) - 1;
                        mpz_add_ui(tmp.get_mpz_t(), tmp.get_mpz_t(), 1);
                        mpz_mul(tmp.get_mpz_t(), tmp.get_mpz_t(), Exponents::POW3_MPZ[limit].get_mpz_t());
                        mpz_tdiv_q_2exp(tmp.get_mpz_t(), tmp.get_mpz_t(), limit);
                        mpz_sub_ui(tmp.get_mpz_t(), tmp.get_mpz_t(), 1);
                        trailing_ones -= limit;
                    }
                    if (trailing_ones > 0) {
                        // tmp = ((CollatzConstants::POW3_MPZ[trailing_ones] * (tmp + 1)) >> trailing_ones) - 1;
                        mpz_add_ui(tmp.get_mpz_t(), tmp.get_mpz_t(), 1);
                        mpz_mul(tmp.get_mpz_t(), tmp.get_mpz_t(), Exponents::POW3_MPZ[trailing_ones].get_mpz_t());
                        mpz_tdiv_q_2exp(tmp.get_mpz_t(), tmp.get_mpz_t(), trailing_ones);
                        mpz_sub_ui(tmp.get_mpz_t(), tmp.get_mpz_t(), 1);
                    }
                    // Now check peak.  However!  We applied an accelerated F(x):  (3x + 1) / 2
                    // Peak was actually tmp * 2.
                    mpz_mul_2exp(tmp_x_2.get_mpz_t(), tmp.get_mpz_t(), 1);
                    if (mpz_cmp(tmp_x_2.get_mpz_t(), out_peak.get_mpz_t()) > 0) {
                        out_peak = tmp_x_2;
                    }
                }
                // Always even at this point.  Shift zeros out.
                right_shifts = count_trailing_zeros(tmp);
                tmp >>= right_shifts;
            }
        } else {
            throw std::runtime_error("Cannot determine data type for st_get_step_count_fast().");
        }
    }



    //
    // For-Each FG Link
    // Generate an FG chain on-the-fly and return the link (F or G) along the way.
    //
    // true == F, false = G
    template<typename Func>
    static void for_each_fg_chain_link(const T& initial_value, Func&& callback) {
        // Do not allow non-ref callbacks.  Otherwise we make GMP over and over.
        static_assert(std::is_invocable_r_v<bool, Func, bool>, "Callback must be callable as bool(bool)");

        // Zero is a special case, mostly for BinaryTree building a root.
        if (initial_value == 0) { return; }

        // Call our basic Collatz iterator.
        bool is_F = false;
        Collatz<T>::for_each_sequence_step(initial_value, [&](const T& current_value) {
            // When the previous step was F, skip the next step which is always the latter part of accerlated F(x).
            if (is_F) {
                // Reset it so we don't loop.
                is_F = false;
                return false;
            } else {
                if constexpr(BuiltinIntegral<T>) {
                    is_F = (current_value & 1) == 1;
                } else if constexpr(GMPIntegral<T>) {
                    is_F = mpz_odd_p(current_value.get_mpz_t());
                }
                bool stop = callback(is_F);
                return stop;
            }
        });
    }



    //
    // For-Each FG Step
    // Steps through a Collatz sequence using F and G steps instead of normal ones.
    //
    template<typename Func>
    static void for_each_fg_step(const T& initial_value, Func&& callback) {
        // Do not allow non-ref callbacks.  Otherwise we make GMP over and over.
        static_assert(std::is_invocable_v<Func, const T&>, "Callback must be callable with (const T&)");

        // Zero is a special case, mostly for BinaryTree building a root.
        if (initial_value == 0) { return; }

        // Call our basic Collatz iterator.
        bool was_F = false;
        Collatz<T>::for_each_sequence_step(initial_value, [&](const T& current_value) {
            // When the previous step was F, skip the next step which is always the latter part of accerlated F(x).
            if (was_F) {
                // Reset it so we don't loop.
                was_F = false;
                return false;
            } else {
                if constexpr(BuiltinIntegral<T>) {
                    was_F = (current_value & 1) == 1;
                } else if constexpr(GMPIntegral<T>) {
                    was_F = mpz_odd_p(current_value.get_mpz_t());
                }
                bool stop = callback(current_value);
                return stop;
            }
        });
    }



    //
    // Get FG Chain String
    // Generate the F-G string for a sequence with some initial value.
    //
    static std::string st_get_fg_chain_string(const T& initial_value, size_t max_chars = std::numeric_limits<size_t>::max()) {
        std::string result;
        size_t count = 0;
        Collatz<T>::for_each_fg_chain_link(initial_value, [&](bool is_F) {
            result += (is_F ? 'F' : 'G');
            count++;
            return count >= max_chars;
        });
        return result;
    }
    //
    // Wrapper for the instance implementation.
    std::string get_fg_pattern_string(size_t max_chars = std::numeric_limits<size_t>::max()) const {
        return Collatz<T>::st_get_fg_chain_string(_initial_value, max_chars);
    }



    //
    // Get OE String
    // Generate the odd-even string for the sequence, which is just an expansion of the F-G string.
    //
    static std::string st_get_oe_pattern_string(const T& initial_value, size_t max_chars = std::numeric_limits<size_t>::max()) {
        std::string fg_pattern = Collatz<T>::st_get_fg_chain_string(initial_value, max_chars);
        return fg_to_oe(fg_pattern, max_chars);
    }
    //
    // Wrapper for the instance implementation.
    std::string get_oe_pattern_string(size_t max_chars = std::numeric_limits<size_t>::max()) {
        return Collatz<T>::st_get_oe_pattern_string(_initial_value, max_chars);
    }



    //
    // FG to OE
    // Convert an FG string to an OE string.
    //
    static std::string fg_to_oe(
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

        // If we didn't hit the max_chars, remove the last E.  Why?  Because all sequences end in 1 (odd) and therefore get an "F" -> "OE"
        // But that last "E" is 1 going to 4 (1 * 3 + 1).
        // If the caller forbids this, skip it.
        if (strip_last_e && oe_string.size() < max_oe_chars && oe_string.size() > 0) {
            oe_string.pop_back();
        }

        // If we're over the max_chars requested, trim.
        if (oe_string.size() > max_oe_chars) {
            oe_string.resize(max_oe_chars);
        }

        return oe_string;
    }
};
