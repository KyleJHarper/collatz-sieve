#pragma once

#include "collatz.hpp"
#include "binary_tree_math.hpp"
#include "concepts.hpp"
#include <gmp.h>
#include <cmath>
#include <limits>
#include <stdexcept>
#include "collatz_affine_map.hpp"



//
// Node
// Nodes are the objects within the BinaryTree, which allow us to track the odd-even/f-g patterns and give us the information we
// need without a full Collatz object.
//
// BIG FAT NOTE!
// By default, we OWN the children.  If you delete a node, it will delete the children, which will cascade.  If you want to delete
// a node without affecting its children, call own_children(false).
//
template <AnySupportedIntegral T>
class Node {
    private:
    static constexpr size_t MAX_CHILDREN = 2;
    static inline std::string E_NO_METADATA_TRACKING = "You disabled metadata when you created this object.";
    // Object members.
    // Memory packing and alignment matter!  Keep this class LIGHT.
    // All data must fit within one cache line.
    //                                                       uint64_t | total | uint128_t | total | mpz_class | total
    T _value;                                            //         8 |     8 |        16 |    16 |        16 |    16
    Node *_parent = nullptr;                             //         8 |    16 |         8 |    24 |         8 |    24
    Node *_hwm_ancestor = nullptr;                       //         8 |    24 |         8 |    32 |         8 |    32
    Node *_children[MAX_CHILDREN] = {nullptr, nullptr};  //        16 |    40 |        16 |    48 |        16 |    48
    bool _is_below_hwm : 1 = false;                      //       1:1 |    41 |       1:1 |    49 |       1:1 |    49
    bool _has_hwm_ancestor : 1 = false;                  //       1:2 |    41 |       1:2 |    49 |       1:2 |    49
    bool _is_initialized : 1 = false;                    //       1:3 |    41 |       1:3 |    49 |       1:3 |    49
    bool _owns_children : 1 = true;                      //       1:4 |    41 |       1:4 |    49 |       1:4 |    49  (4 bits padding)
    uint8_t _child_count = 0;                            //         1 |    42 |         1 |    50 |         1 |    50
    uint8_t _fg_chain_length = 0;                        //         1 |    43 |         1 |    51 |         1 |    51
    // Alignment Padding (u128 is 16 bytes)              //         5 |    48 |        13 |    64 |         5 |    56
    // Free Padding to Cacheline                         //        16 |    64 |         0 |    64 |         8 |    64
    // -- Cache Line --



    public:
    //
    // Constructors
    //
    Node() {
        _value= T{};
        _parent = nullptr;
    }
    Node(const T& value, Node *parent = nullptr) {
        init(value, parent);
    }



    //
    // Destructor
    //
    ~Node() {
        release_children();
    }



    //
    // Cout and string-ified methods.
    //
    friend std::ostream& operator<<(std::ostream &os, const Node<T>& m) {
        return os << m._value;
    }



    //
    // Initialize
    // Builds the object, reusing it if necessary.
    //
    void init(const T& value, Node *parent = nullptr) {
        // Reset object if necessary.
        if(_is_initialized) { reset(); }

        // Establish or clear metadata object.  Reset() already cleared it, if it existed.
        _is_initialized = true;
        _value = value;
        _parent = parent;

        // The F-G (and O-E) chain concept is unique to the BinaryTree strategy, which ties Node to BinaryTree rather tightly, but
        // that's okay for now.  Since the F-G chain is always consistent.  It grows by 1 each level.
        static thread_local CollatzAffineMap<T> affine_map;
        affine_map.reset();
        size_t count = 0;
        _fg_chain_length = get_level() - 1;
        Collatz<T>::for_each_fg_chain_link(_value, [&](bool is_F) {
            if (is_F) {
                affine_map.apply_F();
            } else {
                affine_map.apply_G();
            }
            count++;
            return count >= _fg_chain_length;
        });
        // Now decide if we hit HWM using is_below.  Callee handles GMP type to avoid alloc() for us.
        _is_below_hwm = affine_map.is_below(_value);

        // Use our parent to decide who the high-water mark ancestor is, if any.  Since it's a lineage, there's no
        // reason to scan everything manually.  The parent's data is all we need.
        if (parent != nullptr) {
            // Assign whoever the ancestor is, even if it's nullptr.
            _hwm_ancestor = parent->get_hwm_ancestor();
            // If we have an ancestor, just flag our boolean.
            if (_hwm_ancestor != nullptr) {
                _has_hwm_ancestor = true;
            }
            // If we don't have an ancestor, see if the parent can be.
            if (_hwm_ancestor == nullptr && parent->is_below_high_water_mark()) {
                _hwm_ancestor = parent;
                _has_hwm_ancestor = true;
            }
        }
    };



    //
    // Reset to make this act like a new() object.
    //
    void reset() {
        // The _collatz object has its own resetting logic in its init().  Nothing to do here.
        release_children();
        _parent = nullptr;
        _hwm_ancestor = nullptr;
        _is_below_hwm = false;
        _has_hwm_ancestor = false;
        _is_initialized = false;
        _owns_children = true;
    }



    //
    // Add Child
    // Creates a child and assigns it to this parent.
    //
    Node<T>* add_child(T value) {
        Node *child = new Node(value, this);
        _children[_child_count++] = child;
        return child;
    }



    //
    // Assign Child
    // Assign a child.  No safety checks.  Increment counter.
    void assign_child(Node<T>* child) {
        _children[_child_count++] = child;
    }



    //
    // Own Children
    // Gain or relinquish ownership of children, mostly for destruction cascading purposes later.
    //
    void own_children(bool value) {
        _owns_children = value;
    }



    //
    // Release Child
    // Release (delete) a single child if _owns_children is true, based on the object passed in. Compares pointer value to memory.
    //
    void release_child(Node<T>* child) {
        for (size_t i = 0; i < MAX_CHILDREN; i++) {
            // Find the Child
            if (_children[i] == child) {
                // Delete if owned.
                if (_owns_children) {
                    delete _children[i];
                }
                // Null out our reference.
                _children[i] = nullptr;
                // Push any other children toward the front.
                for (size_t j = i + 1; j < MAX_CHILDREN; j++) {
                    _children[j - 1] = _children[j];
                }
                // Decrement the count and leave.
                _child_count--;
                break;
            }
        }
    }



    //
    // Release Children
    // Releases (delete) all children if _owns_children is true.
    //
    void release_children() {
        for (size_t i = 0; i < MAX_CHILDREN; i++) {
            if (_owns_children) {
                delete _children[i];
            }
            _children[i] = nullptr;
        }
        _child_count = 0;
    }



    // Children are usually generated by a BinaryTree, but in free-form mode, allow a node to generate its own children.
    static void st_spawn_children(Node<T>* node) {
        if (node->get_child_count() > 0) {
            throw std::logic_error("You can't spawn children on a node that already has children.");
        }
        T step = BinaryTreeMath<T>::st_step(node->get_level());

        T child_value = node->get_value();
        for (uint8_t i = 0; i < MAX_CHILDREN; i++) {
            // Step forward.  Avoid alloc() with GMP in the += operator.
            if constexpr(BuiltinIntegral<T>) {
                child_value += step;
            } else if constexpr(GMPIntegral<T>) {
                mpz_add(child_value.get_mpz_t(), child_value.get_mpz_t(), step.get_mpz_t());
            }
            Node<T>* child = new Node<T>(child_value, node);
            node->assign_child(child);
        }
    }
    //
    // Instance helper.
    void spawn_children() {
        st_spawn_children(this);
    }



    //
    // Accessors and properties.
    //
    const T& get_value() const { return _value; }
    Node* get_parent() const { return _parent; }
    Node<T>* get_hwm_ancestor() const { return _hwm_ancestor; }
    bool is_below_high_water_mark() const { return _is_below_hwm; }
    bool has_high_water_mark_ancestor() const { return _has_hwm_ancestor; }
    bool is_initialized() const { return _is_initialized; }
    bool does_own_children() const { return _owns_children; }
    const Node<T>* get_child(size_t index) const { return _children[index]; }
    Node<T>* get_child_unsafe(size_t index) { return _children[index]; }
    size_t get_child_count() const { return static_cast<size_t>( _child_count); }
    size_t get_fg_chain_length() const { return static_cast<size_t>( _fg_chain_length); }
    //
    // Tree level and position come from BinaryTree, but we'll make them accessible here.
    size_t get_level() const { return BinaryTreeMath<T>::st_node_level(_value); }
    T get_position() const { return BinaryTreeMath<T>::st_node_position(_value); }



    //
    // HWM data is available in the Collatz object.
    static size_t st_get_hwm_index(T value) {
        Collatz<T> collatz(value, true, true);
        return collatz.get_hwm_index();
    }
    size_t get_hwm_index() const {
        return st_get_hwm_index(_value);
    }
    //
    // Get the F-G chain.  Use the Collatz static method for this.
    std::string get_fg_chain_string() const {
        return Collatz<T>::st_get_fg_chain_string(_value, _fg_chain_length);
    }
    //
    // Get the odd-even chain.  Use the Collatz static method for this.
    std::string get_odd_even_chain_string() const {
        return Collatz<T>::fg_to_oe(get_fg_chain_string(), std::numeric_limits<size_t>::max(), false);
    }



    //
    // Object Size
    // Deeply scan the object.
    //
    size_t deep_size() const {
        size_t total = sizeof(*this);
        return total;
    }
};
