#ifndef SRC_BINARY_TREE_H_
#define SRC_BINARY_TREE_H_

#include <cmath>
#include <gmp.h>
#include <gmpxx.h>
#include "collatz.hpp"
#include "concepts.hpp"
#include <stdexcept>
#include <unordered_map>


//
// A perfect binary tree mapped to powers of two.  This creates a uniform distribution of nodes in
// the N+/Z space (positive integers), which Collatz is concerned.
//

//
// Node
// A node will have a fractional value based on the distribution of the Odds and Evens as we
// process the sequence.  These odd-even chains are evenly patterened as the tree is generated, so
// we don't want to know the full high-water mark (from Collatz()).
//
template <IntegralOrMPZClass T>
class Node {
    private:
    T _value;
    size_t _level;
    size_t _child_count = 0;
    Node *_children[2];
    Node *_parent;
    Node *_hwm_ancestor = nullptr;
    Collatz<T> *_collatz;
    std::string_view _odd_even_chain_view;
    mpz_class _twos_value_mpz_c;
    mpz_class _threes_value_mpz_c;
    mpf_class _fg_n_portion_mpf_c;
    mpf_class _fg_constant_mpf_c;
    mpf_class _fg_total_mpf_c;

    public:
    // Constructor
    Node(T value, Node *parent = nullptr){
        // Set the argument values.
        _value = value;
        _parent = parent;
        // Calculate our level.  Formula: floor(log2(N))
        if constexpr(std::integral<T>) {
            _level = std::floor(std::log2(_value));
        } else if constexpr(std::same_as<T, mpz_class>) {
            _level = mpz_sizeinbase(_value.get_mpz_t(), 2);
        }
        // Build the collatz sequence object.
        _collatz = new Collatz<T>(_value);
        // Leverage our parent's oe-chain to determine ours.
        size_t oe_chain_length = 0;
        if(parent != nullptr) {
            oe_chain_length = parent->get_odd_even_chain_view().length();
        }
        _odd_even_chain_view = _collatz->get_oe_pattern().substr(0, oe_chain_length);
        if(_odd_even_chain_view.empty() || _odd_even_chain_view.back() == 'E') {
            oe_chain_length += 1;
        } else {
            oe_chain_length += 2;
        }
        _odd_even_chain_view = _collatz->get_oe_pattern().substr(0, oe_chain_length);
        // Get the twos and threes values.  We need a float version too.  GMP's operator=() handles this conversion.
        mpz_ui_pow_ui(_twos_value_mpz_c.get_mpz_t(), 2, std::count(_odd_even_chain_view.begin(), _odd_even_chain_view.end(), 'E'));
        mpz_ui_pow_ui(_threes_value_mpz_c.get_mpz_t(), 3, std::count(_odd_even_chain_view.begin(), _odd_even_chain_view.end(), 'O'));
        // Compute the odd-even fractional N portion, the constant, and then tally them up.
        // We need at least 1 float for GMP to handle this as a floating point division.  The mpf_class will get auto-cleaned up at function end.
        mpf_class tmp_threes_mpf_c = _threes_value_mpz_c;
        _fg_n_portion_mpf_c = tmp_threes_mpf_c / _twos_value_mpz_c;
        for(auto c : _odd_even_chain_view) {
            if(c == 'E') {
                _fg_constant_mpf_c = _fg_constant_mpf_c / 2;
            } else {
                _fg_constant_mpf_c = _fg_constant_mpf_c * 3 + 1;
            }
        }
        _fg_total_mpf_c = (_fg_total_mpf_c * _value) + _fg_constant_mpf_c;
        // Find the closest ancestor who hit high-water mark.
        Node *ancestor = parent;
        while(ancestor != nullptr) {
            if(ancestor->is_below_high_water_mark()) {
                _hwm_ancestor = ancestor;
                break;
            }
            ancestor = ancestor->get_parent();
        }
    };

    // Destructor
    ~Node() {
        delete _collatz;
        for(size_t i=0; i<_child_count; i++) {
            delete _children[i];
        }
    }

    // Cout and string-ified methods.
    friend std::ostream& operator<<(std::ostream &os, const Node<T>& m) {
        return os << m._value;
    }

    // Accessors and properties.
    const T& get_value() const {
        return _value;
    }
    Node* get_parent() const {
        return _parent;
    }
    const mpz_class& get_twos_value() const {
        return _twos_value_mpz_c;
    }
    const mpz_class& get_threes_value() const {
        return _threes_value_mpz_c;
    }
    const std::string_view& get_odd_even_chain_view() const {
        return _odd_even_chain_view;
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
    bool is_below_high_water_mark() const {
        if(_fg_total_mpf_c > _value) {
            return true;
        }
        return false;
    }
    bool has_high_water_mark_ancestor() const {
        if(_hwm_ancestor == nullptr) {
            return true;
        }
        return false;
    }
    Node<T>* add_child(T value) {
        Node *child = new Node(value, this);
        _children[_child_count] = child;
        _child_count += 1;
        return child;
    }
};



//
// Binary Tree
//
template<typename T>
class BinaryTree {
    private:
    Node<T> *_root_node = nullptr;
    size_t _max_level = 0;
    std::unordered_map<size_t, std::vector<Node<T>*>> _level_map;

    public:
    // Constructor
    BinaryTree(size_t levels) {
        _root_node = new Node<T>(0);
        _level_map[0].push_back(_root_node);
        for(size_t level = 1; level <= levels; level++){
            this->add_level();
        }
    }
    // Destructor
    // We don't need anything special here.  When _root_node is deleted, recrusion will free them all.

    // Accessors and properties.
    const size_t& get_max_level() const {
        return _max_level;
    }
    Node<T>* get_root_node() const {
        return _root_node;
    }
    const std::unordered_map<size_t, std::vector<Node<T>*>>& get_level_map() const {
        return _level_map;
    }

    // Add a level to the tree.  We simply take the parent nodes and add two children with a steady
    // step value.
    void add_level() {
        // Get the parent and child level IDs.
        size_t parent_level = _max_level;
        size_t child_level = _max_level + 1;
        _max_level++;
        // Each level will double the size of the tree, so we can't rely on size_t if we're going to
        // support GMP-size values.  We need to respect T.
        T step = 0;
        if constexpr(std::integral<T>) {
            step = std::pow(2, parent_level);
        } else if constexpr(std::same_as<T, mpz_class>) {
            mpz_ui_pow_ui(step.get_mpz_t(), 2, parent_level);
        }
        // Loop through the parents to build the children.
        T child_values[2];
        for(Node<T> *parent : _level_map[parent_level]) {
            // Child values are always type T and step away from parent.
            child_values[0] = parent->get_value() + step;
            child_values[1] = child_values[0] + step;
            for(const T &child_value : child_values) {
                Node<T> *child_node = parent->add_child(child_value);
                _level_map[child_level].push_back(child_node);
            }
        }
    }

    // Generate any Node based on its level and position.  It will not be part of any tree.
    // Throws errors when you ask for invalid positions in a node.
    static Node<T>* generate_node_at(size_t level, T position) {
        // Calculate the maximum position and enforce the rules.  We will need the first node's value too.
        T max_position = 0;
        T first_node_value = 0;
        if constexpr(std::integral<T>) {
            max_position = std::pow(2, level);
            first_node_value = std::pow(2, level) - 1;
        } else if constexpr(std::same_as<T, mpz_class>) {
            mpz_ui_pow_ui(max_position.get_mpz_t(), 2, level);
            mpz_ui_pow_ui(first_node_value.get_mpz_t(), 2, level);
            first_node_value = first_node_value - 1;
        }
        if(position > max_position) {
            throw std::runtime_error("Cannot ask for a position outside of a level's limits.");
        }
        if(position < 1) {
            throw std::runtime_error("You cannot specify position 0 or lower (negative).  Positions start at 1 (leftmost).");
        }
        // Increases are simple: S1 = ceil((pos - 1) / 2) * (2^L-1)
        T s1 = 0;
        mpf_class frequency_mpf_c = position - 1;
        frequency_mpf_c = frequency_mpf_c / 2;
        mpf_ceil(frequency_mpf_c.get_mpf_t(), frequency_mpf_c.get_mpf_t());
        T value = 0;
        T magnitude = 0;
        if constexpr(std::integral<T>) {
            value = std::pow(2, level - 1);
            s1 = frequency_mpf_c.get_d() * value;
        } else if constexpr(std::same_as<T, mpz_class>) {
            mpz_ui_pow_ui(value.get_mpz_t(), 2, level - 1);
            s1 = frequency_mpf_c * value;
        }
        // Decreases require a sigma-style summation, so we loop here.
        // Formula: [n=2, to L=level] 𝝨 ceil((pos - 2^(n-1)) / 2^n) * (2^n - 3) * 2^(L-n)
        T s2 = 0;
        frequency_mpf_c = 0;
        mpz_class tmp_mpz_c = 0;
        for(size_t n=2; n<level; n++) {
            // Frequency.
            mpz_ui_pow_ui(tmp_mpz_c.get_mpz_t(), 2, n - 1);
            frequency_mpf_c = position - tmp_mpz_c;
            mpz_ui_pow_ui(tmp_mpz_c.get_mpz_t(), 2, n);
            frequency_mpf_c = frequency_mpf_c / tmp_mpz_c;
            mpf_ceil(frequency_mpf_c.get_mpf_t(), frequency_mpf_c.get_mpf_t());
            // Value and Magnitude.
            if constexpr(std::integral<T>) {
                value = std::pow(2, n) - 3;
                magnitude = std::pow(2, level - n);
                s2 = s2 + (frequency_mpf_c.get_d() * value * magnitude);
            } else if constexpr(std::same_as<T, mpz_class>) {
                mpz_ui_pow_ui(value.get_mpz_t(), 2, n);
                value = value - 3;
                mpz_ui_pow_ui(magnitude.get_mpz_t(), 2, level - n);
                s2 = s2 + (frequency_mpf_c * value * magnitude);
            }
        }
        // Now sum the values, create the node, and return it.
        T node_value = first_node_value + s1 - s2;
        Node<T>* node = new Node<T>(node_value);
        return node;
    }
};

#endif
