#ifndef SRC_BINARY_TREE_H_
#define SRC_BINARY_TREE_H_

#include <string>
#include <cmath>
#include "collatz.hpp"


// A perfect binary tree mapped to powers of two.  This creates a uniform distribution of nodes in
// the N+/Z space (positive integers), which Collatz is concerned.

template <typename T>
class Node {
    private:
    T _value;
    uint _level;
    Node *_children[2];
    Node *_parent;
    Collatz<T> *_collatz;

    public:
    Node(T value, Node *parent = NULL){
        _value = value;
        _parent = parent;
        _level = std::floor(std::log2(_value));
        _collatz = new Collatz<T>(_value);
    };

    // Cout and string-ified methods.
    friend std::ostream& operator<<(std::ostream &os, const Node<T>& m) {
        return os << m._value;
    }

    // Accessors and properties.
    const T& get_value() {
        return _value;
    }
};

#endif
