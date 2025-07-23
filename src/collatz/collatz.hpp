#ifndef SRC_COLLATZ_H_
#define SRC_COLLATZ_H_

#include <stdexcept>
#include <vector>
#include <string>


// Basic Collatz sequence object with minimal data to keep it tight.
// This class only works with types that support direct arithmetic, such as int, uint, int128_t,
// and so forth.  Specficially, GMP-style math requires function calls, which can't be modeled in
// a template class like this.
template <typename T>
class Collatz {
    private:
    T _initial_value;
    std::vector<T> _sequence;
    std::string _oe_pattern;
    size_t _hwm_index = 0;

    public:
    Collatz(T initial_value){
        if (initial_value < 0) {
            throw std::runtime_error("You cannot create a Collatz sequence with a value lower than 1.");
        }
        _initial_value = initial_value;
        // Build the sequence and its related metadata.
        T current = _initial_value;
        do {
            _sequence.push_back(current);
            if(current % 2 == 0) {
                _oe_pattern.append("E");
                current = current / 2;
            } else {
                _oe_pattern.append("O");
                current = current * 3 + 1;
            }
            // If the next value is going to be lower, use current vector size as index.
            if(_hwm_index == 0 && current < _initial_value) {
                _hwm_index = _sequence.size();
            }
        } while(current > 1 && _initial_value > 1);
        // Now add element '1', unless the IV was 1 or 0.
        if(_initial_value > 1) {
            _sequence.push_back(1);
            _oe_pattern.append("O");
        }
        // Finally, if the IV is 0, set OE to blank.
        if(_initial_value == 0) {
            _oe_pattern.clear();
        }
    };

    // Cout and string-ified methods.
    friend std::ostream& operator<<(std::ostream &os, const Collatz<T>& m) {
        return os << m._initial_value;
    }
    std::string get_sequence_string() {
        std::string rv;
        rv.append(std::to_string(_initial_value));
        for(size_t i=1; i<_sequence.size(); i++) {
            rv.append(", ");
            rv.append(std::to_string(_sequence[i]));
        }
        return rv;
    }

    // Accessors and properties.
    const T& get_initial_value() {
        return _initial_value;
    }
    const std::vector<T>& get_sequence() {
        return _sequence;
    };
    const std::string& get_oe_pattern() {
        return _oe_pattern;
    }
    const size_t& get_hwm_index() {
        return _hwm_index;
    }
    size_t get_stop_count() {
        return _sequence.size();
    }
};

#endif
