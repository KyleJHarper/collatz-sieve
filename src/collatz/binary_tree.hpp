#ifndef SRC_BINARY_TREE_H_
#define SRC_BINARY_TREE_H_

#include <cmath>
#include <gmp.h>
#include <gmpxx.h>
#include "collatz.hpp"


// A perfect binary tree mapped to powers of two.  This creates a uniform distribution of nodes in
// the N+/Z space (positive integers), which Collatz is concerned.
//
// A node will have a fractional value based on the distribution of the Odds and Evens as we
// process the sequence.  These odd-even chains are evenly patterened as the tree is generated, so
// we don't want to know the full high-water mark (from Collatz()).
template <typename T>
class Node {
    private:
    T _value;
    size_t _level;
    size_t _child_count = 0;
    Node *_children[2];
    Node *_parent;
    Node *_hwm_ancestor = NULL;
    Collatz<T> *_collatz;
    std::string_view _odd_even_chain_view;
    mpz_t _twos_value_mpz;
    mpz_t _threes_value_mpz;
    mpf_t _fg_n_portion_mpf;
    mpf_t _fg_constant_mpf;
    mpf_t _fg_total_mpf;

    public:
    // Constructor
    Node(T value, Node *parent = NULL){
        // Set the argument values.
        _value = value;
        _parent = parent;
        // Calculate our level.  Formula: floor(log2(N))
        _level = std::floor(std::log2(_value));
        // Build the collatz sequence object.
        _collatz = new Collatz<T>(_value);
        // Leverage our parent's oe-chain to determine ours.
        size_t oe_chain_length = 0;
        if(parent != NULL) {
            oe_chain_length = parent->get_odd_even_chain_view().length();
        }
        _odd_even_chain_view = _collatz->get_oe_pattern().substr(0, oe_chain_length);
        if(_odd_even_chain_view.empty() || _odd_even_chain_view.back() == 'E') {
            oe_chain_length += 1;
        } else {
            oe_chain_length += 2;
        }
        _odd_even_chain_view = _collatz->get_oe_pattern().substr(0, oe_chain_length);
        // Get the twos and threes values.
        mpz_init(_twos_value_mpz);
        mpz_init(_threes_value_mpz);
        mpz_ui_pow_ui(_twos_value_mpz, 2, std::count(_odd_even_chain_view.begin(), _odd_even_chain_view.end(), 'E'));
        mpz_ui_pow_ui(_threes_value_mpz, 3, std::count(_odd_even_chain_view.begin(), _odd_even_chain_view.end(), 'O'));
        // Compute the odd-even fractional N portion.  Requires floats for this.
        mpf_t twos_value_tmp_mpf;
        mpf_t threes_value_tmp_mpf;
        mpf_init(twos_value_tmp_mpf);
        mpf_init(threes_value_tmp_mpf);
        mpf_set_z(twos_value_tmp_mpf, _twos_value_mpz);
        mpf_set_z(threes_value_tmp_mpf, _threes_value_mpz);
        mpf_init(_fg_n_portion_mpf);
        mpf_init(_fg_constant_mpf);
        mpf_div(_fg_n_portion_mpf, threes_value_tmp_mpf, twos_value_tmp_mpf);
        mpf_clear(twos_value_tmp_mpf);
        mpf_clear(threes_value_tmp_mpf);
        // Now calculate the constant portion.
        for(auto c : _odd_even_chain_view) {
            if(c == 'E') {
                mpf_div_ui(_fg_constant_mpf, _fg_constant_mpf, 2);
            } else {
                mpf_mul_ui(_fg_constant_mpf, _fg_constant_mpf, 3);
                mpf_add_ui(_fg_constant_mpf, _fg_constant_mpf, 1);
            }
        }
        // Multiply out and sum them for the total.
        mpf_init(_fg_total_mpf);
        mpf_mul_ui(_fg_total_mpf, _fg_total_mpf, _value);
        mpf_add(_fg_total_mpf, _fg_total_mpf, _fg_constant_mpf);
        // Find the closest ancestor who hit high-water mark.
        Node *ancestor = parent;
        while(ancestor != NULL) {
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
        mpz_clear(_twos_value_mpz);
        mpz_clear(_threes_value_mpz);
        mpf_clear(_fg_n_portion_mpf);
        mpf_clear(_fg_constant_mpf);
        mpf_clear(_fg_total_mpf);
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
    const mpz_t& get_twos_value() const {
        return _twos_value_mpz;
    }
    const mpz_t& get_threes_value() const {
        return _threes_value_mpz;
    }
    const std::string_view& get_odd_even_chain_view() const {
        return _odd_even_chain_view;
    }
    const mpf_t& get_fg_n_portion() const {
        return _fg_n_portion_mpf;
    }
    const mpf_t& get_fg_constant() const {
        return _fg_constant_mpf;
    }
    const mpf_t& get_fg_total() const {
        return _fg_total_mpf;
    }
    bool is_below_high_water_mark() const {
        if(mpf_cmp_ui(_fg_total_mpf, _value) > 0) {
            return true;
        }
        return false;
    }
    Node* add_child(T value) {
        Node *child = new Node(value, this);
        _children[_child_count] = child;
        _child_count += 1;
        return child;
    }
};

#endif
