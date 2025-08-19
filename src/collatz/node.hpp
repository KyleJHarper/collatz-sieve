#pragma once

#include "collatz.hpp"
#include "binary_tree_math.hpp"
#include "concepts.hpp"
#include <gmp.h>
#include <cmath>
#include <limits>
#include <stdexcept>
#include "gmp_helpers.hpp"



// We need a forward declaration for NodeMetadata to see Node.
template<IntegralOrMPZClass T>
class Node;


//
// Node Metadata
//
// Similar to CollatzMetadata, we offload stuff here and disable it by default, saving memory.
//
template<IntegralOrMPZClass T>
class NodeMetadata {
    static_assert(std::is_integral<T>::value || std::is_same<T, mpz_class>::value, "T must be an integral or mpz_class type");

    private:
    // None, just make them public.

    public:
    Node<T> *hwm_ancestor = nullptr;
    mpz_class fg_twos_value_mpz_c = 0;
    mpz_class fg_threes_value_mpz_c = 0;
    mpf_class fg_n_portion_mpf_c = 0;
    mpf_class fg_constant_mpf_c = 0;
    mpf_class fg_total = 0;
    NodeMetadata() {}
    void reset() {
        hwm_ancestor = nullptr;
        fg_twos_value_mpz_c = 0;
        fg_threes_value_mpz_c = 0;
        fg_n_portion_mpf_c = 0;
        fg_constant_mpf_c = 0;
        fg_total = 0;
    }
    size_t deep_size() const {
        size_t total = sizeof(*this);
        if constexpr(std::is_same<T, mpz_class>::value) {
            total += gmp_deep_sizeof(fg_twos_value_mpz_c);
            total += gmp_deep_sizeof(fg_threes_value_mpz_c);
            total += gmp_deep_sizeof(fg_n_portion_mpf_c);
            total += gmp_deep_sizeof(fg_constant_mpf_c);
            total += gmp_deep_sizeof(fg_total);
        }
        return total;
    }
};



//
// Node
// A node will have a fractional value based on the distribution of the Odds and Evens as we process the sequence.  These odd-even
// chains are evenly patterned as the tree is generated, so we don't want to know the full high-water mark (from Collatz()).
//
// BIG FAT NOTE!
// By default, we OWN the children.  If you delete a node, it will delete the children, which will cascade.  If you want to delete
// a node without affecting its children, call own_children(false).
//
template <IntegralOrMPZClass T>
class Node {
    private:
    static constexpr size_t MAX_CHILDREN = 2;
    static inline std::string E_NO_METADATA_TRACKING = "You disabled metadata when you created this object.";
    // Make some thread-local MPZ and MPF items so they're safe for re-use with threading.
    static inline thread_local mpz_class tls_twos_value_mpz_c;
    static inline thread_local mpz_class tls_threes_value_mpz_c;
    static inline thread_local mpf_class tls_threes_value_mpf_c;
    static inline thread_local mpf_class tls_fg_n_portion_mpf_c;
    static inline thread_local mpf_class tls_fg_constant_mpf_c;
    static inline thread_local mpf_class tls_fg_total_mpf_c;
    // Object members.
    // Memory packing and alignment matter!  Keep this class LIGHT unless the caller wants metadata.
    // All data must fit within one cache line.
    //                                                       uint64_t | total | mpz_class | total
    T _value;                                            //         8 |     8 |        16 |    16
    Node *_parent = nullptr;                             //         8 |    16 |         8 |    24
    Node *_hwm_ancestor = nullptr;                       //         8 |    24 |         8 |    32
    Node *_children[MAX_CHILDREN] = {nullptr, nullptr};  //        16 |    40 |        16 |    48
    NodeMetadata<T>* _metadata = nullptr;                //         8 |    48 |         8 |    56
    bool _is_below_hwm : 1 = false;                      //       1:1 |    49 |       1:1 |    57
    bool _has_hwm_ancestor : 1 = false;                  //       1:2 |    49 |       1:2 |    57
    bool _is_initialized : 1 = false;                    //       1:3 |    49 |       1:3 |    57
    bool _owns_children : 1 = true;                      //       1:4 |    49 |       1:4 |    57
    bool _track_metadata : 1 = false;                    //       1:5 |    49 |       1:5 |    57  (3 bits padding)
    uint8_t _child_count = 0;                            //         1 |    50 |         1 |    58
    uint8_t _fg_chain_length = 0;                        //         1 |    51 |         1 |    59
    // Alignment Padding                                 //         5 |    56 |         5 |    64
    // Free Padding to Cacheline                         //         8 |    64 |         0 |    64
    // -- Cache Line --



    public:
    //
    // Constructors
    //
    Node() {
        _value= T{};
        _parent = nullptr;
    }
    Node(const T& value, bool track_metadata, Node *parent = nullptr) {
        init(value, track_metadata, parent);
    }



    //
    // Destructor
    //
    ~Node() {
        release_children();
        release_metadata();
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
    void init(const T& value, bool track_metadata, Node *parent = nullptr) {
        // Reset object if necessary.
        if(_is_initialized) { reset(); }

        // Establish or clear metadata object.  Reset() already cleared it, if it existed.
        if (_metadata == nullptr && track_metadata) { _metadata = new NodeMetadata<T>(); }
        if (_metadata != nullptr && ! track_metadata) { release_metadata(); }
        _is_initialized = true;
        _track_metadata = track_metadata;
        _value = value;
        _parent = parent;

        // The F-G (and O-E) chain concept is unique to the BinaryTree strategy, which ties Node to BinaryTree rather tightly, but
        // that's okay for now.  Since the F-G chain is always consistent.  It grows by 1 each level.
        _fg_chain_length = get_level();
        std::string fg_chain = Collatz<T>::st_get_fg_pattern_string(_value, _fg_chain_length);
        _fg_chain_length = fg_chain.size();

        // Calculating twos, threes, FG data, and is_hwm uses thread_locals.  Reset/use them wisely!
        //   > Get the twos and threes values.  We need a float version too.  GMP's operator=() handles this conversion.
        //   > Compute the odd-even fractional N portion, the constant, and then tally them up.
        //   > We need at least 1 float for GMP to handle this as a floating point division.
        //   > Odd count is number of F's, and even count is just size().
        size_t odd_count = std::count(fg_chain.begin(), fg_chain.end(), 'F');
        mpz_pow_ui(tls_twos_value_mpz_c.get_mpz_t(), CollatzConstants::MPZ_TWO.get_mpz_t(), fg_chain.size());  // 2^even_count
        mpz_pow_ui(tls_threes_value_mpz_c.get_mpz_t(), CollatzConstants::MPZ_THREE.get_mpz_t(), odd_count);    // 3^odd_count
        tls_threes_value_mpf_c = tls_threes_value_mpz_c;
        tls_fg_n_portion_mpf_c = tls_threes_value_mpf_c / tls_twos_value_mpz_c;
        tls_fg_constant_mpf_c = 0;
        for (char& c : fg_chain) {
            if (c == 'G') {
                mpf_mul(tls_fg_constant_mpf_c.get_mpf_t(), tls_fg_constant_mpf_c.get_mpf_t(), CollatzConstants::MPF_HALF.get_mpf_t());   // tls_fg_constant_mpf_c /= 2
            } else {
                mpf_mul(tls_fg_constant_mpf_c.get_mpf_t(), tls_fg_constant_mpf_c.get_mpf_t(), CollatzConstants::MPF_THREE.get_mpf_t());  // tls_fg_constant_mpf_c *= 3
                mpf_add(tls_fg_constant_mpf_c.get_mpf_t(), tls_fg_constant_mpf_c.get_mpf_t(), CollatzConstants::MPF_ONE.get_mpf_t());    // tls_fg_constant_mpf_c += 1
                mpf_mul(tls_fg_constant_mpf_c.get_mpf_t(), tls_fg_constant_mpf_c.get_mpf_t(), CollatzConstants::MPF_HALF.get_mpf_t());   // tls_fg_constant_mpf_c /= 2
            }
        }
        tls_fg_total_mpf_c = (tls_fg_n_portion_mpf_c * _value) + tls_fg_constant_mpf_c;
        if (tls_fg_total_mpf_c < _value) { _is_below_hwm = true; }
        if (_track_metadata) {
            _metadata->fg_twos_value_mpz_c = tls_twos_value_mpz_c;
            _metadata->fg_threes_value_mpz_c = tls_threes_value_mpf_c;
            _metadata->fg_n_portion_mpf_c = tls_fg_n_portion_mpf_c;
            _metadata->fg_constant_mpf_c = tls_fg_constant_mpf_c;
            _metadata->fg_total = tls_fg_total_mpf_c;
        }

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
    // Add Child
    // Creates a child and assigns it to this parent.
    //
    Node<T>* add_child(T value) {
        Node *child = new Node(value, _track_metadata, this);
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
            if constexpr(std::same_as<T, mpz_class>) {
                mpz_add(child_value.get_mpz_t(), child_value.get_mpz_t(), step.get_mpz_t());
            } else {
                child_value += step;
            }
            Node<T>* child = new Node<T>(child_value, node->get_track_metadata(), node);
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
    bool get_track_metadata() const { return _track_metadata; }
    const Node<T>* get_child(size_t index) const { return _children[index]; }
    Node<T>* get_child_unsafe(size_t index) { return _children[index]; }
    size_t get_child_count() const { return static_cast<size_t>( _child_count); }
    size_t get_fg_chain_length() const { return static_cast<size_t>( _fg_chain_length); }
    //
    // Tree level and position come from BinaryTree, but we'll make them accessible here.
    size_t get_level() const { return BinaryTreeMath<T>::st_node_level(_value); }
    T get_position() const { return BinaryTreeMath<T>::st_node_position(_value); }



    //
    // Metadata Accessors
    //
    const mpz_class& get_twos_value() const {
        if (! _track_metadata) {
            throw std::logic_error(E_NO_METADATA_TRACKING);
        }
        return _metadata->fg_twos_value_mpz_c;
    }
    const mpz_class& get_threes_value() const {
        if (! _track_metadata) {
            throw std::logic_error(E_NO_METADATA_TRACKING);
        }
        return _metadata->fg_threes_value_mpz_c;
    }
    const mpf_class& get_fg_n_portion() const {
        if (! _track_metadata) {
            throw std::logic_error(E_NO_METADATA_TRACKING);
        }
        return _metadata->fg_n_portion_mpf_c;
    }
    const mpf_class& get_fg_constant() const {
        if (! _track_metadata) {
            throw std::logic_error(E_NO_METADATA_TRACKING);
        }
        return _metadata->fg_constant_mpf_c;
    }
    const mpf_class& get_fg_total() const {
        if (! _track_metadata) {
            throw std::logic_error(E_NO_METADATA_TRACKING);
        }
        return _metadata->fg_total;
    }



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
        return Collatz<T>::st_get_fg_pattern_string(_value, _fg_chain_length);
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
        if (_metadata != nullptr) {
            total += _metadata->deep_size();
        }
        return total;
    }
};
