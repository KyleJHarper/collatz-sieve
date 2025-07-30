#ifndef SRC_COLLATZ_H_
#define SRC_COLLATZ_H_

#include <iostream>
#include <gmpxx.h>
#include <stdexcept>
#include <vector>
#include <string>
#include <stdint.h>
#include "concepts.hpp"



#define PEAK_64BIT_ODD 6148914691236517203
namespace CollatzConstants {
    constexpr bool ODD = true;
    constexpr bool EVEN = false;
}


//
// Basic Collatz sequence object with minimal data to keep it tight.
//
template <IntegralOrMPZClass T>
class Collatz {
    static_assert(std::is_integral<T>::value || std::is_same<T, mpz_class>::value, "T must be an integral or mpz_class type");

    private:
    T _initial_value;
    T _peak_value = 0;
    std::vector<T> _sequence;
    std::vector<bool> _oe_pattern;
    size_t _hwm_index = 0;
    size_t _step_count = 0;
    bool _must_reset = false;
    bool _track_sequence = false;

    public:
    inline static bool detect_overflow = false;
    // Constructors.  Offload to init() so objects can be reused.
    Collatz() {}
    Collatz(T initial_value, bool track_sequence = false) {
        _track_sequence = track_sequence;
        init(initial_value);
    };
    void init(T initial_value) {
        if (initial_value < 0) {
            throw std::runtime_error("You cannot create a Collatz sequence with a value lower than 0.");
        }
        // Reset if necessary.
        if(_must_reset) {
            _sequence.clear();
            _peak_value = 0;
            _oe_pattern.clear();
            _hwm_index = 0;
            _step_count = 0;
        }
        _must_reset = true;
        // Now process the new value.
        _initial_value = initial_value;
        // Build the sequence (optional) and its related metadata.
        T current = _initial_value;
        do {
            _step_count++;
            if(_track_sequence) {
                _sequence.emplace_back(current);
            }
            if(current > _peak_value) {
                _peak_value = current;
            }
            if(current % 2 == 0) {
                _oe_pattern.push_back(CollatzConstants::EVEN);
                current /= 2;
            } else {
                if constexpr(std::integral<T>) {
                    if (Collatz::detect_overflow) {
                        if(current > PEAK_64BIT_ODD) {
                            std::string msg;
                            msg += "While building the sequence for initial value ";
                            msg += std::to_string(_initial_value);
                            msg += " we reached a step whose value exceeds the limit.";
                            msg += "  Its value is " + std::to_string(current);
                            msg += ", but the limit to remain under 2^64 (UINT64_MAX) is ";
                            msg += std::to_string(PEAK_64BIT_ODD);
                            msg += "  We must abort because this would cause an integer overflow.";
                            throw std::runtime_error(msg);
                        }
                    }
                }
                _oe_pattern.push_back(CollatzConstants::ODD);
                current *= 3;
                current += 1;
            }
            // If the next value is going to be lower, mark the index.
            if(_hwm_index == 0 && current < _initial_value) {
                _hwm_index = _step_count;
            }
        } while(current > 1 && _initial_value > 1);
        // Now add element '1', unless the IV was 1 or 0.
        if(_initial_value > 1) {
            if(_track_sequence) {
                _sequence.push_back(1);
            }
            _oe_pattern.push_back(CollatzConstants::ODD);
            _step_count++;
        }
        // Finally, if the IV is 0, set OE to blank.
        if(_initial_value == 0) {
            _oe_pattern.clear();
            _step_count = 0;
        }
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

    // Accessors and properties.
    const T& get_initial_value() const {
        return _initial_value;
    }
    const T& get_peak_value() const {
        return _peak_value;
    }
    const std::vector<T>& get_sequence() const {
        if(!_track_sequence) {
            throw std::logic_error("You disabled sequence tracking when you created this object.");
        }
        return _sequence;
    };
    const std::vector<bool>& get_oe_pattern() const {
        return _oe_pattern;
    }
    std::string get_oe_pattern_string() const {
        std::string result;
        result.reserve(_oe_pattern.size());
        for (bool bit : _oe_pattern) {
            result += bit == CollatzConstants::ODD ? 'O' : 'E';
        }
        return result;
    }
    const size_t& get_hwm_index() const {
        return _hwm_index;
    }
    size_t get_step_count() const {
        return _step_count;
    }
    size_t get_sequence_size() const {
        return _step_count + 1;
    }
    void clear_sequence() {
        _sequence.clear();
    }
    size_t deep_size() const {
        size_t total = 0;
        total += sizeof(*this);
        for (auto &stop : _sequence) {
            total += sizeof(stop);
        }
        total += sizeof(T) * _sequence.capacity();
        // Vector<bool> is a specialized template in c++.  Bit-packed.
        total += ((_oe_pattern.capacity() + 7) / 8);
        return total;
    }

};

#endif
