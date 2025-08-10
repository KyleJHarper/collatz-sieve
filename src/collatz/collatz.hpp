#pragma once

#include <concepts>
#include <gmp.h>
#include <iostream>
#include <gmpxx.h>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include <string>
#include <stdint.h>
#include "concepts.hpp"
#include "gmp_helpers.hpp"


// Custom Exceptions
class CollatzSequenceOverflow : public std::runtime_error {
    public:
    explicit CollatzSequenceOverflow(const std::string& msg) : std::runtime_error(msg) {}
};

// Sequences are not size_t in length.  Use a common type that's smaller.
typedef uint32_t seq_size_t;

// Constants for our sequences.
namespace CollatzConstants {
    // GMP will sometimes alloc() if you operate on a non-GMP (e.g.: ui) value.
    static const mpz_class MPZ_ONE = 1;
    static const mpz_class MPZ_TWO = 2;
    static const mpz_class MPZ_THREE = 3;
    // Let's lock in what "odd" and "even" mean.
    constexpr bool ODD = true;
    constexpr bool EVEN = false;
    // Trying to perform 3X+1 on any (odd) value higher than this would overflow a 64-bit unsigned integer.
    constexpr uint64_t MAX_64BIT_ODD = 6148914691236517203;
}


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
template<IntegralOrMPZClass T>
class CollatzMetadata {
    static_assert(std::is_integral<T>::value || std::is_same<T, mpz_class>::value, "T must be an integral or mpz_class type");

    private:
    // None, just make them public.

    public:
    T peak_value;
    seq_size_t hwm_index = 0;
    seq_size_t step_count = 0;
    CollatzMetadata() {}
    void reset() {
        peak_value = 0;
        hwm_index = 0;
        step_count = 0;
    }
    size_t deep_size() const {
        size_t total = sizeof(*this);
        if constexpr(std::is_same<T, mpz_class>::value) {
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
template<IntegralOrMPZClass T>
class Collatz {
    static_assert(std::is_integral<T>::value || std::is_same<T, mpz_class>::value, "T must be an integral or mpz_class type");

    private:
    // Memory packing and alignment matter!  Keep this class LIGHT unless the caller wants metadata.
    // All data must fit within one cache line.
    //                                 uint64_t | total | mpz_class | total
    T _initial_value;                         //        8 |     8 |        16 |    16
    bool _is_initialized : 1;                 //      1:1 |     9 |       1:1 |    17
    bool _track_sequence : 1;                 //      1:2 |     9 |       1:2 |    17
    bool _track_metadata : 1;                 //      1:3 |     9 |       1:3 |    17
    bool _sequence_overflow : 1;              //      1:4 |     9 |       1:4 |    17  (4 bits padding)
    // Alignment Padding                      //        7 |    16 |         7 |    24
    std::vector<T> _sequence;                 //       24 |    40 |        24 |    48
    CollatzMetadata<T>* _metadata = nullptr;  //        8 |    48 |         8 |    56
    // Free Padding to Cacheline              //       16 |    64 |         8 |    64

    public:
    // Reusable messages.
    static inline std::string E_NO_SEQUENCE_TRACKING = "You disabled sequence tracking when you created this object.";
    static inline std::string E_NO_METADATA_TRACKING = "You disabled metadata when you created this object.";
    // Constructors.  Offload to init() so objects can be reused.
    Collatz() {}
    Collatz(T initial_value, bool track_sequence = false, bool track_metadata = false) {
        init(initial_value, track_sequence, track_metadata);
    };
    // Destructor
    ~Collatz() {
        release_metadata();
    }

    void init(T initial_value, bool track_sequence = false, bool track_metadata = false) {
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
            } catch(const CollatzSequenceOverflow& err) {
                _sequence_overflow = true;
                throw(err);
            }
            if (_track_sequence) {
                _sequence.shrink_to_fit();
            }
        }
    }

    // Reset to make this act like a new() object.  Do NOT allocate Metadata here.
    void reset() {
        _sequence.clear();
        _sequence.shrink_to_fit();
        _sequence_overflow = false;
        _track_sequence = false;
        _track_metadata = false;
        if (_metadata != nullptr) { _metadata->reset(); }
    }

    // Let callers decide when they're done with metadata.
    void release_metadata() {
        if (_metadata == nullptr) { return; }
        delete _metadata;
        _metadata = nullptr;
        _track_metadata = false;
    }

    // Cout and string-ified methods.
    friend std::ostream& operator<<(std::ostream &os, const Collatz<T>& m) {
        return os << m._initial_value;
    }
    std::string get_sequence_string() {
        std::string rv;
        if constexpr(std::integral<T>) {
            rv.append(std::to_string(_initial_value));
        } else if constexpr(std::same_as<T, mpz_class>) {
            rv.append(_initial_value.get_str());
        }
        for(size_t i=1; i<_sequence.size(); i++) {
            rv.append(", ");
            if constexpr(std::integral<T>) {
                rv.append(std::to_string(_sequence[i]));
            } else if constexpr(std::same_as<T, mpz_class>) {
                rv.append(_sequence[i].get_str());
            }
        }
        return rv;
    }

    // Accessors.
    const T& get_initial_value() const { return _initial_value; }
    bool get_is_overflowed() const { return _sequence_overflow; }
    bool get_is_initialized() const { return _is_initialized; }
    bool get_track_sequence() const { return _track_sequence; }
    bool get_track_metadata() const { return _track_metadata; }
    const CollatzMetadata<T>* get_metadata() const { return _metadata; }
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

    size_t deep_size() const {
        size_t total = sizeof(*this);
        // For _sequence
        if constexpr (std::is_same<T, mpz_class>::value) {
            total += sizeof(mpz_class) * _sequence.capacity();
            for (const auto& val : _sequence) {
                total += gmp_deep_sizeof(val);
            }
        } else {
            total += sizeof(T) * _sequence.capacity();
        }
        if (_metadata != nullptr) {
            total += _metadata->deep_size();
        }
        return total;
    }

    // Run through the sequence with a callback each step.  Will use _sequence if it's populated.
    // Caller MUST return true or false to continue or stop.
    template<typename Func>
    void for_each_sequence_step(Func&& callback) const {
        // Do not allow non-ref callbacks.  Otherwise we make GMP over and over.
        static_assert(std::is_same_v<typename first_arg_type<Func>::type, const T&>, "Callback must be callable with a const T&");

        // Zero is a special case, mostly for BinaryTree building a root.
        if (_initial_value == 0) { return; }

        // Sequence exists?  Use it directly.
        if (_sequence.size() > 0) {
            for (const T& current_step : _sequence) {
                bool stop = callback(current_step);
                if (stop) { return; }
            }
            return;
        }

        // Sequence didn't exist.  Calculate it on-the-fly.
        thread_local T current_step;
        current_step = _initial_value;
        // Native and GMP types differ.  Handle them separately to reduce allocs().
        if constexpr(std::integral<T>) {
            // Native integrals can use intrinsic arithmetic operators for "free", but can overflow.
            while (current_step >= 1) {
                bool stop = callback(current_step);
                if (stop || current_step == 1) { return; }
                if (current_step % 2 == 0) {
                    current_step /= 2;
                } else {
                    if (current_step > CollatzConstants::MAX_64BIT_ODD) {
                        throw CollatzSequenceOverflow("Overflow when building for_each_sequence_step().");
                    }
                    current_step *= 3;
                    current_step += 1;
                }
            }
        } else {
            // GMP integers will alloc() with certain arithmetic operators, but can't overflow.
            while (current_step >= CollatzConstants::MPZ_ONE) {
                bool stop = callback(current_step);
                if (stop || current_step == 1) { return; }
                if (mpz_divisible_p(current_step.get_mpz_t(), CollatzConstants::MPZ_TWO.get_mpz_t())) {
                    current_step /= 2;
                } else {
                    current_step *= 3;
                    current_step += 1;
                }
            }
        }
    }

    // The Odd-Even string for the sequence.
    std::string get_oe_pattern_string(size_t max_chars = std::numeric_limits<size_t>::max()) const {
        std::string result;
        size_t count = 0;
        for_each_sequence_step([&](const T& step) {
            count++;
            result += (step % 2 == 0 ? 'E' : 'O');
            return count >= max_chars;
        });
        return result;
    }

};
