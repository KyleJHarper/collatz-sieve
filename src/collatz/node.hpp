#pragma once

#include "collatz.hpp"
#include "concepts.hpp"
#include <vector>
#include <cmath>
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
    T position;
    Node<T> *hwm_ancestor = nullptr;
    mpz_class fg_twos_value_mpz_c = 0;
    mpz_class fg_threes_value_mpz_c = 0;
    mpf_class fg_n_portion_mpf_c = 0;
    mpf_class fg_constant_mpf_c = 0;
    mpf_class fg_total = 0;
    NodeMetadata() {}
    void reset() {
        position = 0;
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
// A node will have a fractional value based on the distribution of the Odds and Evens as we
// process the sequence.  These odd-even chains are evenly patterned as the tree is generated, so
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
    static inline std::string E_NO_METADATA_TRACKING = "You disabled metadata when you created this object.";
    // Make some thread-local MPZ and MPF items so they're safe for re-use with threading.
    static inline thread_local mpz_class tls_twos_value_mpz_c;
    static inline thread_local mpz_class tls_threes_value_mpz_c;
    static inline thread_local mpf_class tls_threes_value_mpf_c;
    static inline thread_local mpf_class tls_fg_n_portion_mpf_c;
    static inline thread_local mpf_class tls_fg_constant_mpf_c;
    static inline thread_local mpf_class tls_fg_total_mpf_c;
    // Thread-local version of the collatz to prevent alloc on init().
    static inline thread_local Collatz<T>tls_collatz;
    // Object members.
    T _value;
    Node *_parent = nullptr;
    Node *_hwm_ancestor = nullptr;
    Node *_children[MAX_CHILDREN] = {nullptr, nullptr};
    NodeMetadata<T>* _metadata = nullptr;

    bool _is_below_hwm : 1 = false;
    bool _has_hwm_ancestor : 1 = false;
    bool _is_initialized : 1 = false;
    bool _owns_children : 1 = true;
    bool _track_metadata : 1 = false;
    uint8_t _child_count = 0;

    std::vector<bool> _odd_even_chain;   // This is super heavy for what it does ... refactor lifetime?


    public:
    // Constructors
    Node() {
        _value= T{};
        _parent = nullptr;
    }
    Node(T value, bool track_metadata, Node *parent = nullptr) {
        init(value, track_metadata, parent);
    }
    // Use an init so we can reset and reuse objects.
    void init(T value, bool track_metadata, Node *parent = nullptr) {
        // Reset object if necessary.
        if(_is_initialized) { reset(); }
        // Establish or clear metadata object.  Reset() already cleared it, if it existed.
        if (_metadata == nullptr && track_metadata) { _metadata = new NodeMetadata<T>(); }
        if (_metadata != nullptr && ! track_metadata) { release_metadata(); }
        _is_initialized = true;
        _track_metadata = track_metadata;
        _value = value;
        _parent = parent;
        // Rebuild our collatz object.
        tls_collatz.init(_value, false, false);

        // Calculate our position if the parent exists.  Formula: 2 * (parent_position - 1) + [1 or 2]
        if (_track_metadata) {
            if (parent == nullptr) {
                _metadata->position = 1;
            } else {
                _metadata->position = ((parent->get_position() - 1) * 2) + (parent->get_child_count() + 1);
            }
        }

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
            size_t count = 0;
            tls_collatz.for_each_sequence_step([&](const T& step) {
                count++;
                _odd_even_chain.push_back(step % 2 == 0 ? CollatzConstants::EVEN : CollatzConstants::ODD);
                return count >= oe_chain_length;
            });
            // We now have the parent's OE chain, possibly with 2 extra steps.  Trim if parent ended in Even.
            if (_odd_even_chain.size() > 2) {
                if (_odd_even_chain[_odd_even_chain.size() - 3] == CollatzConstants::EVEN) {
                    _odd_even_chain.pop_back();
                }
            }
        }

        // Calculating twos, threes, fg data, and is_hwm uses thread_locals.  Reset/use them wisely!
        // Get the twos and threes values.  We need a float version too.  GMP's operator=() handles this conversion.
        // Compute the odd-even fractional N portion, the constant, and then tally them up.
        // We need at least 1 float for GMP to handle this as a floating point division.  The mpf_class will get auto-cleaned up at function end.
        size_t odd_count = std::count(_odd_even_chain.begin(), _odd_even_chain.end(), CollatzConstants::ODD);
        mpz_ui_pow_ui(tls_twos_value_mpz_c.get_mpz_t(), 2, _odd_even_chain.size() - odd_count);
        mpz_ui_pow_ui(tls_threes_value_mpz_c.get_mpz_t(), 3, odd_count);
        tls_threes_value_mpf_c = tls_threes_value_mpz_c;
        tls_fg_n_portion_mpf_c = tls_threes_value_mpf_c / tls_twos_value_mpz_c;
        tls_fg_constant_mpf_c = 0;
        for (auto c : _odd_even_chain) {
            if (c == CollatzConstants::EVEN) {
                tls_fg_constant_mpf_c /= 2;
            } else {
                tls_fg_constant_mpf_c *= 3;
                tls_fg_constant_mpf_c += 1;
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
            // If we have an ancestor, just flag our boolean and leave.
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

    // Reset to make this act like a new() object.
    void reset() {
        // The _collatz object has its own resetting logic in its init().  Nothing to do here.
        release_children();
        _parent = nullptr;
        _hwm_ancestor = nullptr;
        _odd_even_chain.clear();
        _is_below_hwm = false;
        _has_hwm_ancestor = false;
        _is_initialized = false;
        _owns_children = true;
        _track_metadata = false;
        if (_metadata != nullptr) { _metadata->reset(); }
    }

    // Release children tracking and, if enabled, their memory.
    void release_children() {
        for (size_t i = 0; i < MAX_CHILDREN; i++) {
            if (_owns_children) {
                delete _children[i];
            }
            _children[i] = nullptr;
        }
        _child_count = 0;
    }

    // Let callers decide when they're done with metadata.
    void release_metadata() {
        if (_metadata == nullptr) { return; }
        delete _metadata;
        _metadata = nullptr;
        _track_metadata = false;
    }

    // Destructor
    ~Node() {
        release_children();
        release_metadata();
    }

    // Cout and string-ified methods.
    friend std::ostream& operator<<(std::ostream &os, const Node<T>& m) {
        return os << m._value;
    }

    // Accessors and properties.
    const T& get_value() const {
        return _value;
    }


    // Calculate our level.  Formula: floor(log2(N))
    size_t get_level() const {
        size_t level = 0;
        if constexpr(std::integral<T>) {
            level = std::floor(std::log2(_value));
        } else if constexpr(std::same_as<T, mpz_class>) {
            level = mpz_sizeinbase(_value.get_mpz_t(), 2);
        }
        return level;
    }


    bool is_initialized() const { return _is_initialized; }
    bool is_below_high_water_mark() const { return _is_below_hwm; }
    bool has_high_water_mark_ancestor() const { return _has_hwm_ancestor; }
    Node* get_parent() const { return _parent; }
    bool does_own_children() const { return _owns_children; }
    size_t get_child_count() const { return static_cast<size_t>(_child_count); }
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

    const T& get_position() const {
        if (! _track_metadata) {
            throw std::logic_error(E_NO_METADATA_TRACKING);
        }
        return _metadata->position;
    }
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

    Node<T>* get_hwm_ancestor() const {
        return _hwm_ancestor;
    }
    void assign_child(Node<T>* child) {
        _children[_child_count++] = child;
    }
    Node<T>* add_child(T value) {
        Node *child = new Node(value, _track_metadata, this);
        _children[_child_count++] = child;
        return child;
    }
    void own_children(bool value) {
        _owns_children = value;
    }
    size_t deep_size() const {
        size_t total = sizeof(*this);
        // Vector<bool> is a specialized template in c++.  Bit-packed.
        total += (_odd_even_chain.capacity() + 7) / 8;
        if (_metadata != nullptr) {
            total += _metadata->deep_size();
        }
        return total;
    }
};
