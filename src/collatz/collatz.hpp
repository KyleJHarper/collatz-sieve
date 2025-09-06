#pragma once

#include <gmp.h>
#include <iostream>
#include <gmpxx.h>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include <array>
#include <string>
#include <stdint.h>
#include "concepts.hpp"
#include "gmp_helpers.hpp"



//
// Custom Exceptions
//
class CollatzSequenceOverflow : public std::runtime_error {
    public:
    explicit CollatzSequenceOverflow(const std::string& msg) : std::runtime_error(msg) {}
};



//
// Sequences are not size_t in length.  Use a common type that's smaller.
//
typedef uint32_t seq_size_t;



//
// Constants and helpers for our sequences.
//
namespace CollatzConstants {
    // GMP will sometimes alloc() if you operate on a non-GMP (e.g.: ui) value.
    static const mpz_class MPZ_ONE = 1;
    static const mpz_class MPZ_TWO = 2;
    static const mpz_class MPZ_THREE = 3;

    // GMP float-style values used in a lot of calculations.
    static const mpf_class MPF_ONE = 1;
    static const mpf_class MPF_TWO = 2;
    static const mpf_class MPF_THREE = 3;
    static const mpf_class MPF_HALF = 0.5;

    // Let's lock in what "odd" and "even" mean.
    constexpr bool ODD = true;
    constexpr bool EVEN = false;

    // Trying to perform 3X+1 on any value higher than this would overflow a 64-bit unsigned integer.
    constexpr std::array<uint128_t, 5> MAX_3XP1 = {
        ((uint128_t(1) <<  8) - 1 - 1) / 3,   // 8 bit
        ((uint128_t(1) << 16) - 1 - 1) / 3,   // 16 bit
        ((uint128_t(1) << 32) - 1 - 1) / 3,   // 32 bit
        ((uint128_t(1) << 64) - 1 - 1) / 3,   // 64 bit
        ((uint128_t(1) << 127) + ((uint128_t(1) << 127) - 1) - 1) / 3  // 128 bit
        // Requires a little juggling to avoid overflow.  Yay PEMDAS!
    };
    //
    // Now a helper for it.
    template<BuiltinIntegral T>
    inline constexpr uint128_t get_max_3xp1() {
        switch (sizeof(T) * 8) {
            case   8: return MAX_3XP1[0];
            case  16: return MAX_3XP1[1];
            case  32: return MAX_3XP1[2];
            case  64: return MAX_3XP1[3];
            case 128: return MAX_3XP1[4];
            default: throw std::logic_error("Bit size not supported: " + std::to_string(sizeof(T) * 8));
        }
    }

    // Precomputed maximum initial values for a given bit size.  The next value would overflow during its sequence.
    constexpr std::array<uint64_t, 65> MAX_INITIAL_VALUE_BY_64BIT = {
        0,  // 0
        2,  // 1
        2,  // 2
        2,  // 3
        6,  // 4
        6,  // 5
        14,  // 6
        14,  // 7
        26,  // 8
        26,  // 9
        26,  // 10
        26,  // 11
        26,  // 12
        26,  // 13
        446,  // 14
        446,  // 15
        702,  // 16
        702,  // 17
        1818,  // 18
        1818,  // 19
        1818,  // 20
        4254,  // 21
        4254,  // 22
        9662,  // 23
        9662,  // 24
        20894,  // 25
        26622,  // 26
        60974,  // 27
        60974,  // 28
        60974,  // 29
        77670,  // 30
        113382,  // 31
        159486,  // 32
        159486,  // 33
        159486,  // 34
        665214,  // 35
        1042430,  // 36
        1212414,  // 37
        2684646,  // 38
        3041126,  // 39
        4637978,  // 40
        5656190,  // 41
        6416622,  // 42
        6631674,  // 43
        6631674,  // 44
        6631674,  // 45
        19638398,  // 46
        19638398,  // 47
        19638398,  // 48
        80049390,  // 49
        80049390,  // 50
        120080894,  // 51
        210964382,  // 52
        319804830,  // 53
        319804830,  // 54
        319804830,  // 55
        319804830,  // 56
        319804830,  // 57
        319804830,  // 58
        319804830,  // 59
        319804830,  // 60
        1410123942,  // 61
        1410123942,  // 62
        8528817510,  // 63
        12327829502,  // 64
    };
    constexpr std::array<uint128_t, 65> MAX_INITIAL_VALUE_BY_128BIT = {
        0,  // 0
        2,  // 1
        2,  // 2
        2,  // 3
        6,  // 4
        6,  // 5
        14,  // 6
        14,  // 7
        26,  // 8
        26,  // 9
        26,  // 10
        26,  // 11
        26,  // 12
        26,  // 13
        446,  // 14
        446,  // 15
        702,  // 16
        702,  // 17
        1818,  // 18
        1818,  // 19
        1818,  // 20
        4254,  // 21
        4254,  // 22
        9662,  // 23
        9662,  // 24
        20894,  // 25
        26622,  // 26
        60974,  // 27
        60974,  // 28
        60974,  // 29
        77670,  // 30
        113382,  // 31
        159486,  // 32
        159486,  // 33
        159486,  // 34
        665214,  // 35
        1042430,  // 36
        1212414,  // 37
        2684646,  // 38
        3041126,  // 39
        4637978,  // 40
        5656190,  // 41
        6416622,  // 42
        6631674,  // 43
        6631674,  // 44
        6631674,  // 45
        19638398,  // 46
        19638398,  // 47
        19638398,  // 48
        80049390,  // 49
        80049390,  // 50
        120080894,  // 51
        210964382,  // 52
        319804830,  // 53
        319804830,  // 54
        319804830,  // 55
        319804830,  // 56
        319804830,  // 57
        319804830,  // 58
        319804830,  // 59
        319804830,  // 60
        1410123942,  // 61
        1410123942,  // 62
        8528817510,  // 63
        12327829502,  // 64
        // uint128_t(1) << 127,  // 65
        // uint128_t(1) << 127,  // 66
        // uint128_t(1) << 127,  // 67
        // uint128_t(1) << 127,  // 68
        // uint128_t(1) << 127,  // 69
        // uint128_t(1) << 127,  // 70
        // uint128_t(1) << 127,  // 71
        // uint128_t(1) << 127,  // 72
        // uint128_t(1) << 127,  // 73
        // uint128_t(1) << 127,  // 74
        // uint128_t(1) << 127,  // 75
        // uint128_t(1) << 127,  // 76
        // uint128_t(1) << 127,  // 77
        // uint128_t(1) << 127,  // 78
        // uint128_t(1) << 127,  // 79
        // uint128_t(1) << 127,  // 80
        // uint128_t(1) << 127,  // 81
        // uint128_t(1) << 127,  // 82
        // uint128_t(1) << 127,  // 83
        // uint128_t(1) << 127,  // 84
        // uint128_t(1) << 127,  // 85
        // uint128_t(1) << 127,  // 86
        // uint128_t(1) << 127,  // 87
        // uint128_t(1) << 127,  // 88
        // uint128_t(1) << 127,  // 89
        // uint128_t(1) << 127,  // 90
        // uint128_t(1) << 127,  // 91
        // uint128_t(1) << 127,  // 92
        // uint128_t(1) << 127,  // 93
        // uint128_t(1) << 127,  // 94
        // uint128_t(1) << 127,  // 95
        // uint128_t(1) << 127,  // 96
        // uint128_t(1) << 127,  // 97
        // uint128_t(1) << 127,  // 98
        // uint128_t(1) << 127,  // 99
        // uint128_t(1) << 127,  // 100
        // uint128_t(1) << 127,  // 101
        // uint128_t(1) << 127,  // 102
        // uint128_t(1) << 127,  // 103
        // uint128_t(1) << 127,  // 104
        // uint128_t(1) << 127,  // 105
        // uint128_t(1) << 127,  // 106
        // uint128_t(1) << 127,  // 107
        // uint128_t(1) << 127,  // 108
        // uint128_t(1) << 127,  // 109
        // uint128_t(1) << 127,  // 110
        // uint128_t(1) << 127,  // 111
        // uint128_t(1) << 127,  // 112
        // uint128_t(1) << 127,  // 113
        // uint128_t(1) << 127,  // 114
        // uint128_t(1) << 127,  // 115
        // uint128_t(1) << 127,  // 116
        // uint128_t(1) << 127,  // 117
        // uint128_t(1) << 127,  // 118
        // uint128_t(1) << 127,  // 119
        // uint128_t(1) << 127,  // 120
        // uint128_t(1) << 127,  // 121
        // uint128_t(1) << 127,  // 122
        // uint128_t(1) << 127,  // 123
        // uint128_t(1) << 127,  // 124
        // uint128_t(1) << 127,  // 125
        // uint128_t(1) << 127,  // 126
        // uint128_t(1) << 127,  // 127
        // uint128_t(1) << 127,  // 128
    };
    //
    // Max Check
    template<AnySupportedIntegral T>
    inline constexpr size_t get_max_initial_value_max_bits() {
        if constexpr(NativeIntegral<T>) {
            return MAX_INITIAL_VALUE_BY_64BIT.size() - 1;
        } else if constexpr(ExtendedIntegral<T> || GMPIntegral<T>) {
            return MAX_INITIAL_VALUE_BY_128BIT.size() - 1;
        }
    }
    //
    // Lookup
    template<AnySupportedIntegral T>
    inline constexpr T get_max_initial_value_by_bit(size_t bit_size) {
        // Safety Check
        if (bit_size > get_max_initial_value_max_bits<T>()) {
            throw std::out_of_range("Max initial value for bit size " + std::to_string(bit_size) + " not found.");
        }

        // Pick the right type.
        if constexpr(NativeIntegral<T>) {
            return MAX_INITIAL_VALUE_BY_64BIT[bit_size];
        } else if constexpr(ExtendedIntegral<T>) {
            return MAX_INITIAL_VALUE_BY_128BIT[bit_size];
        } else if constexpr(GMPIntegral<T>) {
            return uint128_to_mpz(MAX_INITIAL_VALUE_BY_128BIT[bit_size]);
        }
        throw std::logic_error("Unknown type for bit required.");
    }
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
    //                                           uint64_t | total | mpz_class | total
    T _initial_value;                         //        8 |     8 |        16 |    16
    bool _is_initialized : 1;                 //      1:1 |     9 |       1:1 |    17
    bool _track_sequence : 1;                 //      1:2 |     9 |       1:2 |    17
    bool _track_metadata : 1;                 //      1:3 |     9 |       1:3 |    17
    bool _sequence_overflow : 1;              //      1:4 |     9 |       1:4 |    17  (4 bits padding)
    // Alignment Padding                      //        7 |    16 |         7 |    24
    std::vector<T> _sequence;                 //       24 |    40 |        24 |    48
    CollatzMetadata<T>* _metadata = nullptr;  //        8 |    48 |         8 |    56
    // Free Padding to Cacheline              //       16 |    64 |         8 |    64
    // Max uintxxx_t to fit: 192
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
                _metadata->step_count--;
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
    // For-Each Step
    // Run through the sequence with a callback each step.  Caller MUST return true or false to continue or stop.
    //
    template<typename Func>
    static void for_each_sequence_step(const T& initial_value, Func&& callback) {
        // Do not allow non-ref callbacks.  Otherwise we make GMP over and over.
        static_assert(std::is_same_v<typename first_arg_type<Func>::type, const T&>, "Callback must be callable with a const T&");

        // Zero is a special case, mostly for BinaryTree building a root.
        if (initial_value == 0) { return; }

        thread_local T current_step;
        current_step = initial_value;
        if constexpr(BuiltinIntegral<T>) {
            // Fixed integrals can use intrinsic arithmetic operators for "free", but can overflow.
            while (current_step >= 1) {
                bool stop = callback(current_step);
                if (stop || current_step == 1) { return; }
                if (current_step % 2 == 0) {
                    current_step /= 2;
                } else {
                    if (current_step > CollatzConstants::get_max_3xp1<T>()) {
                        throw CollatzSequenceOverflow("Overflow when building for_each_sequence_step().");
                    }
                    current_step *= 3;
                    current_step += 1;
                }
            }
        } else if constexpr(GMPIntegral<T>) {
            // GMP integers will alloc() with certain arithmetic operators, but can't overflow.
            while (current_step >= CollatzConstants::MPZ_ONE) {
                bool stop = callback(current_step);
                if (stop || current_step == 1) { return; }
                if (mpz_even_p(current_step.get_mpz_t())) {
                    mpz_tdiv_q_2exp(current_step.get_mpz_t(), current_step.get_mpz_t(), 1);  // current_step >> 1   ==>  current_step /= 2
                } else {
                    mpz_mul(current_step.get_mpz_t(), current_step.get_mpz_t(), CollatzConstants::MPZ_THREE.get_mpz_t());  // current_step *= 3
                    mpz_add(current_step.get_mpz_t(), current_step.get_mpz_t(), CollatzConstants::MPZ_ONE.get_mpz_t());    // current_step += 1
                }
            }
        }
    }
    //
    // Wrapper for the instance implementation.
    template<typename Func>
    void for_each_sequence_step(Func&& callback) const {
        // Do not allow non-ref callbacks.  Otherwise we make GMP over and over.
        static_assert(std::is_same_v<typename first_arg_type<Func>::type, const T&>, "Callback must be callable with a const T&");

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
    // Returns the step count for any initial value.  Recall it's sequence - 1.
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
    // Get FG String
    // Generate the F-G string for a sequence with some initial value.
    //
    static std::string st_get_fg_pattern_string(const T& initial_value, size_t max_chars = std::numeric_limits<size_t>::max()) {
        std::string result;
        size_t count = 0;
        bool skip = false;
        Collatz<T>::for_each_sequence_step(initial_value, [&](const T& step) {
            // When the previous was 'F' (odd), we need to skip the next, because F is a full Odd-Even pair.
            if (skip) {
                skip = false;
            } else {
                count++;
                if constexpr(BuiltinIntegral<T>) {
                    result += (step % 2 == 0 ? 'G' : 'F');
                } else if constexpr(GMPIntegral<T>) {
                    result += (mpz_even_p(step.get_mpz_t()) ? 'G' : 'F');
                }
                skip = result.back() == 'F';
            }
            return count >= max_chars;
        });
        return result;
    }
    //
    // Wrapper for the instance implementation.
    std::string get_fg_pattern_string(size_t max_chars = std::numeric_limits<size_t>::max()) const {
        return Collatz<T>::st_get_fg_pattern_string(_initial_value, max_chars);
    }



    //
    // Get OE String
    // Generate the odd-even string for the sequence, which is just an expansion of the F-G string.
    //
    static std::string st_get_oe_pattern_string(const T& initial_value, size_t max_chars = std::numeric_limits<size_t>::max()) {
        std::string fg_pattern = Collatz<T>::st_get_fg_pattern_string(initial_value, max_chars);
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
