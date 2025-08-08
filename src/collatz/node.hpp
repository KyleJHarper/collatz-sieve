#pragma once

#include "collatz.hpp"
#include "concepts.hpp"
#include <vector>
#include <cmath>
#include "gmp_helpers.hpp"


//
// Node
// A node will have a fractional value based on the distribution of the Odds and Evens as we
// process the sequence.  These odd-even chains are evenly patterened as the tree is generated, so
// we don't want to know the full high-water mark (from Collatz()).
//
// BIG FAT NOTE!
// By default, we OWN the children.  If you delete a node, it will delete the children, which will cascade.
// If you want to delete a node without affecting its children, call own_children(false).
//
template <IntegralOrMPZClass T>
class Node {
    private:
    static constexpr size_t MAX_CHILDREN = 2;
    T _value;
    size_t _level;
    size_t _position = 0;
    size_t _child_count = 0;
    Node *_children[MAX_CHILDREN] = {nullptr, nullptr};
    Node *_parent = nullptr;
    Node *_hwm_ancestor = nullptr;
    Collatz<T> _collatz;
    std::vector<bool> _odd_even_chain;
    mpz_class _twos_value_mpz_c;
    mpz_class _threes_value_mpz_c;
    mpf_class _fg_n_portion_mpf_c;
    mpf_class _fg_constant_mpf_c;
    mpf_class _fg_total_mpf_c;
    bool _is_initialized = false;
    bool _own_children = true;
    static inline bool keep_sequences = false;

    public:
    // Constructors
    Node() {
        _value= T{};
        _parent = nullptr;
    }
    Node(T value, Node *parent = nullptr) {
        init(value, parent);
    }
    // Use an init so we can reset and reuse objects.
    void init(T value, Node *parent = nullptr) {
        // Reset if necessary.
        if(_is_initialized) {
            // The _collatz object has its own resetting logic in its init().  Nothing to do here.
            _level = 0;
            _position = 0;
            release_children();
            _parent = nullptr;
            _hwm_ancestor = nullptr;
            _odd_even_chain.clear();
            _twos_value_mpz_c = 0;
            _threes_value_mpz_c = 0;
            _fg_n_portion_mpf_c = 0;
            _fg_constant_mpf_c = 0;
            _fg_total_mpf_c = 0;
        }
        _is_initialized = true;
        // Set the argument values.
        _value = value;
        _parent = parent;
        // Calculate our level.  Formula: floor(log2(N))
        if constexpr(std::integral<T>) {
            _level = std::floor(std::log2(_value));
        } else if constexpr(std::same_as<T, mpz_class>) {
            _level = mpz_sizeinbase(_value.get_mpz_t(), 2);
        }
        // Calculate our position if the parent exists.  Formula: 2 * (parent_position - 1) + [1 or 2]
        if(parent != nullptr) {
            size_t child_position = 1;
            if(parent->get_child_count() > 0) {
                child_position = 2;
            }
            _position = ((parent->get_position() - 1) * 2) + child_position;
        } else {
            _position = 1;
        }
        // Build the collatz sequence object.
        _collatz.init(_value, keep_sequences);
        // Leverage our parent's oe-chain size to determine ours.  When zero, there's no chain to get.
        // We don't want to double-scan, so we'll employ optimism and pull back locally.
        if (_value > 0) {
            size_t oe_chain_length = 0;
            if (parent == nullptr) {
                oe_chain_length = 1;
            } else {
                oe_chain_length = parent->get_odd_even_chain().size() + (_value > 2 ? 2 : 1);
            }
            _odd_even_chain.reserve(oe_chain_length);
            _collatz.for_each_odd_even_bit(oe_chain_length, [&](bool bit) {
                _odd_even_chain.push_back(bit);
            });
            // We now have the parent's OE chain, possibly with 2 extra steps.  Trim if parent ended in Even.
            if (_odd_even_chain.size() > 2) {
                if (_odd_even_chain[_odd_even_chain.size() - 3] == CollatzConstants::EVEN) {
                    _odd_even_chain.pop_back();
                }
            }
        }
        // Get the twos and threes values.  We need a float version too.  GMP's operator=() handles this conversion.
        mpz_ui_pow_ui(_twos_value_mpz_c.get_mpz_t(), 2, std::count(_odd_even_chain.begin(), _odd_even_chain.end(), CollatzConstants::EVEN));
        mpz_ui_pow_ui(_threes_value_mpz_c.get_mpz_t(), 3, std::count(_odd_even_chain.begin(), _odd_even_chain.end(), CollatzConstants::ODD));
        // Compute the odd-even fractional N portion, the constant, and then tally them up.
        // We need at least 1 float for GMP to handle this as a floating point division.  The mpf_class will get auto-cleaned up at function end.
        mpf_class tmp_threes_mpf_c = _threes_value_mpz_c;
        _fg_n_portion_mpf_c = tmp_threes_mpf_c / _twos_value_mpz_c;
        _fg_constant_mpf_c = 0;
        for(auto c : _odd_even_chain) {
            if(c == CollatzConstants::EVEN) {
                _fg_constant_mpf_c = _fg_constant_mpf_c / 2;
            } else {
                _fg_constant_mpf_c = _fg_constant_mpf_c * 3 + 1;
            }
        }
        _fg_total_mpf_c = (_fg_n_portion_mpf_c * _value) + _fg_constant_mpf_c;
        // Find the closest ancestor who hit high-water mark, even if we hit it ourself too.
        // This will keep track of the earliest ancestor for any depth of nodes.
        // We don't have to scan all ancestors either: we can simply assign the parent's ancestor
        // and then, if it's null, scan the tree for one.
        if(parent != nullptr) {
            _hwm_ancestor = parent->get_hwm_ancestor();
        }
        if(_hwm_ancestor == nullptr) {
            Node *ancestor = parent;
            while(ancestor != nullptr) {
                if(ancestor->is_below_high_water_mark()) {
                    _hwm_ancestor = ancestor;
                    break;
                }
                ancestor = ancestor->get_parent();
            }
        }
    };
    // Release children tracking and, if enabled, their memory.
    void release_children() {
        for (size_t i = 0; i < MAX_CHILDREN; i++) {
            if (_own_children) {
                delete _children[i];
            }
            _children[i] = nullptr;
        }
        _child_count = 0;
    }

    // Destructor
    ~Node() {
        // delete _collatz;  Not needed.  Collatz is inlined onto the stack.
        release_children();
    }

    // Cout and string-ified methods.
    friend std::ostream& operator<<(std::ostream &os, const Node<T>& m) {
        return os << m._value;
    }

    // Accessors and properties.
    const T& get_value() const {
        return _value;
    }
    const size_t& get_level() const {
        return _level;
    }
    bool is_initialized() const {
        return _is_initialized;
    }
    const size_t& get_position() const {
        return _position;
    }
    Node* get_parent() const {
        return _parent;
    }
    bool get_own_children() const {
        return _own_children;
    }
    const size_t& get_child_count() const {
        return _child_count;
    }
    const Collatz<T>& get_collatz() const {
        return _collatz;
    }
    const mpz_class& get_twos_value() const {
        return _twos_value_mpz_c;
    }
    const mpz_class& get_threes_value() const {
        return _threes_value_mpz_c;
    }
    const std::vector<bool>& get_odd_even_chain() const {
        return _odd_even_chain;
    }
    std::string get_odd_even_chain_string() const {
        std::string result;
        result.reserve(_odd_even_chain.size());
        for (bool bit : _odd_even_chain) {
            result += (bit ? 'O' : 'E');
        }
        return result;
    }
    const mpf_class& get_fg_n_portion() const {
        return _fg_n_portion_mpf_c;
    }
    const mpf_class& get_fg_constant() const {
        return _fg_constant_mpf_c;
    }
    const mpf_class& get_fg_total() const {
        return _fg_total_mpf_c;
    }
    Node<T>* get_hwm_ancestor() const {
        return _hwm_ancestor;
    }
    bool is_below_high_water_mark() const {
        if(_fg_total_mpf_c < _value) {
            return true;
        }
        return false;
    }
    bool has_high_water_mark_ancestor() const {
        if(_hwm_ancestor == nullptr) {
            return false;
        }
        return true;
    }
    void assign_child(Node<T>* child) {
        _children[_child_count++] = child;
    }
    Node<T>* add_child(T value) {
        Node *child = new Node(value, this);
        _children[_child_count++] = child;
        return child;
    }
    void own_children(bool value) {
        _own_children = value;
    }
    size_t deep_size() const {
        size_t total = sizeof(*this);
        // The _collatz is inlined.  Add its deep size, but subtract is shallow size.
        total += _collatz.deep_size();
        total -= sizeof(_collatz);
        // Vector<bool> is a specialized template in c++.  Bit-packed.
        total += (_odd_even_chain.capacity() + 7) / 8;
        if constexpr (std::same_as<T, mpz_class>) {
            total += gmp_deep_sizeof(_twos_value_mpz_c);
            total += gmp_deep_sizeof(_threes_value_mpz_c);
            total += gmp_deep_sizeof(_fg_n_portion_mpf_c);
            total += gmp_deep_sizeof(_fg_constant_mpf_c);
            total += gmp_deep_sizeof(_fg_total_mpf_c);
        }
        return total;
    }
    static void enable_sequenes() {
        Node<T>::keep_sequences = true;
    }
    static void disable_sequenes() {
        Node<T>::keep_sequences = false;
    }
};
