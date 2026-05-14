#pragma once

#include "collatz.hpp"
#include "binary_tree_math.hpp"
#include "collatz_constants.hpp"
#include "concepts.hpp"
#include "string.hpp"
#include <cstdint>
#include <gmp.h>
#include <cmath>
#include <limits>
#include <string>
#include "collatz_affine_stride.hpp"
#include "stream_helper.hpp"
#include "equality_helper.hpp"



/**
* @class Node
* @brief The value and metadata for a node inside of a `BinaryTree`.
*
* Nodes are the objects within a `BinaryTreeMaterializedImpl` or the workhorses used when building a `BinaryTreeImplicitImpl` via
* the `BinaryTree` facade.
*
* @tparam T Any supported integral (see concepts.hpp).
*/
template <AnySupportedIntegral T>
class Node {
    private:
    /// @brief Maximum amount of children a node is allowed to have.
    static constexpr size_t MAX_CHILDREN = 2;
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
    // Alignment Padding (u128 is 16 bytes)              //         6 |    48 |        14 |    64 |         6 |    56
    // Free Padding to Cacheline                         //        16 |    64 |         0 |    64 |         8 |    64
    // -- Cache Line --



    public:
    /// @name Lifecycle Management
    /// @{

    /// @brief Default constructor.
    Node() {
        _value= T{};
        _parent = nullptr;
    }



    /// @brief Construct which allows value specified and parent optionally set when initialized.
    Node(const T& value, Node *parent = nullptr) {
        init(value, parent);
    }



    /// @brief Disallow copying.
    Node(const Node&) = delete;
    /// @brief Disallow copying.
    Node& operator=(const Node&) = delete;



    /// @brief Moving is okay.
    Node(Node&&) noexcept = default;
    /// @brief Moving is okay.
    Node& operator=(Node&&) noexcept = default;



    /// @brief Destructor, which simply needs to call `release_children()` in case it owns any which need freed.
    ~Node() {
        release_children();
    }



    /**
    * @brief Initializes a node object with `value` and an optional parent, treating it like it's new.
    *
    * This method is by far one of the hottest paths when building any tree type.  It handles type-specific optimizations for you,
    * and even handles overflow detection as well.  It can be called reliably for any value.
    *
    * This method leans on `init_sequence_helper()` for the overflow detection and affine stride optimization.
    *
    * @param value Reference to the value for this node.
    * @param parent A pointer to a parent, if one exists.  This only establishes a one-way connection: it does not asked `parent`
    * to taken ownership of this (child) object.  Caller must handle this.
    */
    void init(const T& value, Node *parent = nullptr) {
        // Reset object if necessary.
        if(_is_initialized) { reset(); }

        // Establish or clear metadata object.  Reset() already cleared it, if it existed.
        _is_initialized = true;
        if constexpr(FixedWidthIntegral<T>) {
            _value = value;
        } else if constexpr (GMPIntegral<T>) {
            mpz_set(_value.get_mpz_t(), value.get_mpz_t());
        }
        _parent = parent;

        // Call a helper with a promoted type to process the sequence and build our FG/HWM info.  Doing this allows trees to build
        // up to 2^<bit-1>.
        if constexpr(FixedWidthIntegral<T>) {
            if constexpr (sizeof(T) * 8 <= 64) {
                // Dealing with T of 64 bits or less.  If it'll overflow, use 128-bit.
                if (_value <= CollatzConstants::get_max_initial_value_by_bit<uint64_t>(64)) {
                    init_sequence_helper<uint64_t>();
                } else {
                    init_sequence_helper<uint128_t>();
                }
            } else if constexpr(sizeof(T) * 8 == 128) {
                // Dealing with 128 bits.  If it'll overflow, use mpz_class.
                if (_value <= CollatzConstants::get_max_initial_value_by_bit<uint128_t>(128)) {
                    init_sequence_helper<uint128_t>();
                } else {
                    init_sequence_helper<mpz_class>();
                }
            }
        } else if constexpr(GMPIntegral<T>) {
            // Dealing with mpz_class.  Send as-is.
            init_sequence_helper<mpz_class>();
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



    /**
    * @brief Helper used exclusively by `init()` for type comparison when processing sequence for FG chain/HWM analysis.
    * @tparam U Any supported integral (see concepts.hpp).
    */
    template<AnySupportedIntegral U>
    inline void init_sequence_helper() {
        // Cache the FG chain length.
        const size_t fg_chain_length = get_fg_chain_length();

        // We need to elevate _value to type U in case it's not the same.
        static thread_local U u_value;
        static thread_local U u_current_value;
        if constexpr(FixedWidthIntegral<T> && FixedWidthIntegral<U>) {
            // Both types are fixed-width.  They can cast directly, even if they're the same.  Dirt cheap, so don't overthink this.
            u_value = U(_value);
            u_current_value = u_value;
        } else if constexpr(FixedWidthIntegral<T> && GMPIntegral<U>) {
            // Node is fixed-width (probably uint128_t) but required U is mpz_class.  Elevate.
            Int128::uint128_to_mpz(_value, u_value);
            u_current_value = u_value;
        } else if constexpr(GMPIntegral<T> && GMPIntegral<U>) {
            // Both are GMP.  Just set, which is cheap.
            mpz_set(u_value.get_mpz_t(), _value.get_mpz_t());
            mpz_set(u_current_value.get_mpz_t(), u_value.get_mpz_t());
        }

        // Now loop and stride.
        size_t strides = fg_chain_length / AffineStride::STRIDE_SIZE;
        size_t steps_taken = strides * AffineStride::STRIDE_SIZE;
        while (strides > 0) {
            AffineStride::apply_stride(u_current_value);
            strides -= 1;
        }

        // Mop up leftovers after striding.
        while (steps_taken < fg_chain_length) {
            // We're going to shift no matter what, so all we're checking for is odd.
            if constexpr(FixedWidthIntegral<T>) {
                // If it's odd, F step.
                if (u_current_value % 2 == 1) {
                    u_current_value = (u_current_value << 1) + u_current_value + 1;
                }
            } else if constexpr (GMPIntegral<T>) {
                // If it's odd, F step.
                if (mpz_odd_p(u_current_value.get_mpz_t())) {
                    mpz_mul(u_current_value.get_mpz_t(), u_current_value.get_mpz_t(), CollatzConstants::MPZ_THREE.get_mpz_t());
                    mpz_add(u_current_value.get_mpz_t(), u_current_value.get_mpz_t(), CollatzConstants::MPZ_ONE.get_mpz_t());
                }
            }
            // Always even at this point.  Shift by CTZ, clamped by FG chain length.
            // This technique isn't faster for 64/128-bit types, but it's faster for GMP, so we'll do it.
            size_t zeros_count = std::min(static_cast<size_t>(Bit::count_trailing_zeros(u_current_value)), fg_chain_length - steps_taken);
            if constexpr(FixedWidthIntegral<T>) {
                u_current_value >>= zeros_count;
            } else {
                mpz_tdiv_q_2exp(u_current_value.get_mpz_t(), u_current_value.get_mpz_t(), zeros_count);
            }
            steps_taken += zeros_count;
        }

        // Now compare the current value to our original value.
        _is_below_hwm = u_current_value < u_value;
    }



    /// @brief Reset to make this act like a new() object.
    void reset() {
        release_children();
        _parent = nullptr;
        _hwm_ancestor = nullptr;
        _is_below_hwm = false;
        _has_hwm_ancestor = false;
        _is_initialized = false;
        _owns_children = true;
    }

    /// @}



    /// @brief Cout and string-ified methods.
    friend std::ostream& operator<<(std::ostream &os, const Node<T>& m) {
        return os << m._value;
    }



    /// @name Child-Related Accessors
    /// @{

    /**
    * @brief Create a node object with `value` and assign it to this member's `_children` list.
    * @param value Value of the child.
    * @return Pointer to the child.
    */
    Node<T>* add_child(T value) {
        Node *child = new Node(value, this);
        _children[_child_count++] = child;
        return child;
    }



    /// @brief Assign a child to this `_children` list, with no safety checks.
    void assign_child(Node<T>* child) {
        _children[_child_count++] = child;
    }



    /// @brief Gain or relinquish ownership of children, mostly for destruction cascading purposes later.
    void own_children(bool value) {
        _owns_children = value;
    }



    /// @brief Release (delete) a single child if `_owns_children` is true.
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



    /// @brief Releases (delete) all children if `_owns_children` is true.
    void release_children() {
        for (size_t i = 0; i < MAX_CHILDREN; i++) {
            if (_owns_children) {
                delete _children[i];
            }
            _children[i] = nullptr;
        }
        _child_count = 0;
    }



    /// @brief Returns true if this object "owns" children (for destruction purposes), false otherwise.
    bool does_own_children() const { return _owns_children; }



    /// @brief Return a readonly pointer to a child.
    const Node<T>* get_child(size_t index) const { return _children[index]; }



    /// @brief Return a read-write pointer to a child.
    Node<T>* get_child_unsafe(size_t index) { return _children[index]; }



    /// @brief Return the number of children assigned to this node.
    uint8_t get_child_count() const { return _child_count; }

    /// @}



    /// @name Parent- and Ancestor-Related Accessors
    /// @{

    /// @brief Assign the `parent` to this `_parent`, with no safety checks.
    void assign_parent(Node<T>* parent) {
        _parent = parent;
    }



    /// @brief Return a readonly pointer to the parent.
    Node* get_parent() const { return _parent; }



    /// @brief Assign the `hwm_ancestor` to this `_hwm_ancestor`, with no safety checks.
    void assign_hwm_ancestor(Node<T>* hwm_ancestor) {
        _hwm_ancestor = hwm_ancestor;
    }



    /// @brief Return a readonly pointer to the HWM ancestor.
    Node<T>* get_hwm_ancestor() const { return _hwm_ancestor; }



    /**
    * @brief Return the sequence index for the HWM of a given value.
    * @note HWM data is available in the Collatz object.
    * @param value The value to process a sequence for.
    * @return The index where High-Water Mark is reached.
    */
    static seq_size_t st_get_hwm_index(T value) {
        Collatz<T> collatz(value, true, true);
        return collatz.get_hwm_index();
    }



    /// @brief Member helper to get this node's High-Water Mark index.  See `st_get_hwm_index()`.
    seq_size_t get_hwm_index() const {
        return st_get_hwm_index(_value);
    }

    /// @}



    /// @name Accessors
    /// @{

    /// @brief Return this node's value.
    const T& get_value() const { return _value; }



    /// @brief Flag to determine if this node is below the High-Water Mark.
    bool is_below_high_water_mark() const { return _is_below_hwm; }



    /// @brief Flag to determine if this node has a HWM ancestor.
    bool has_high_water_mark_ancestor() const { return _has_hwm_ancestor; }



    /// @brief Flag to determine if this node has been initialized with a value.
    bool is_initialized() const { return _is_initialized; }



    /// @brief Return the size of the FG chain for this node.  Uses `BinaryTreeMath::st_fg_chain_length()` under the hood.
    seq_size_t get_fg_chain_length() const { return BinaryTreeMath<T>::st_fg_chain_length(get_level()); }



    /// @brief Return this node's level.  It's calculated from `BinaryTreeMath::st_get_level_by_node_value()`.
    level_t get_level() const { return BinaryTreeMath<T>::st_get_level_by_node_value(_value); }



    /// @brief Return the position of this node.  Uses `BinaryTreeMath::st_node_position()` under the hood.
    T get_position() const { return BinaryTreeMath<T>::st_node_position(_value); }



    /// @brief Get the F-G chain for this node.  Uses `Collatz::st_get_fg_chain_string()`.
    std::string get_fg_chain_string() const {
        return Collatz<T>::st_get_fg_chain_string(_value, get_fg_chain_length());
    }



    /// @brief Get the odd-even (OE) chain for this node.  Uses `Collatz::fg_to_oe()`.
    std::string get_odd_even_chain_string() const {
        return Collatz<T>::st_fg_to_oe(get_fg_chain_string(), std::numeric_limits<size_t>::max(), false);
    }

    /// @}



    /**
    * @brief Calculate the size of this data structure as closely as possible.
    * @return The size in bytes of the object.
    */
    size_t deep_size() const {
        size_t total = sizeof(*this);
        return total;
    }



    //
    // Equal
    // Compares this node to "other".  Returns true if equal.
    //
    // Returns true if they are equal in representation.  False otherwise.
    // Will explain what failed to *err if sent.
    //
    /**
    * @brief Compare two Nodes' specific internals and return true if identical.
    *
    * This function checks:
    *   1. Pointers null agree.
    *   2. Node values match.
    *   3. Parent's null agree.  If they exist, their values match.
    *   4. HWM Ancestor's flags and pointers null agree.  If they exist, their values match.
    *   5. Children have matching counts, null agreement, and equal values.
    *   6. Flags for _is_below_hwm match.
    *   7. Flags for _is_initialized match.
    *   8. FG chain lengths and strings match.
    *
    * @param first The first Node to compare.
    * @param second The second Node to compare.
    * @param err Pointer to a string where inequality or error messages are stored.
    * @return True if equal, false otherwise.
    */
    static bool st_equal (const Node<T>* first, const Node<T>* second, std::string* err = nullptr) {
        EqualityHelper eq(err);
        eq.set_category("Node");

        // They should both be real objects or nullptr.
        // When they disagree because one is real and one is a nullptr, fail.
        if (! eq.pointers_null_agree(first, second)) {
            return eq.fail("Node objects don't agree on null state");
        }

        // If they're null, we're done.
        if (first == nullptr) {
            return true;
        }

        // If they both have the same value, they're either both nullptr or pointing to the same node, which is fine.
        if (first == second) {
            return true;
        }

        // At this point, we have two objects that null-agree but are different, so they must be real objects.  We have to assume
        // they might be coming from different collections or trees, so we have to compare values, not pointers.

        // Values
        if (eq.unequal(first->get_value(), second->get_value())) {
            return eq.fail("Node values mismatch");
        }

        // Parents
        if (! eq.pointers_null_agree(first->get_parent(), second->get_parent())) {
            return eq.fail("Parent nodes' pointers don't agree on null state");
        }
        if (first->get_parent() != nullptr) {
            if (eq.unequal(first->get_parent()->get_value(), second->get_parent()->get_value())) {
                return eq.fail("Nodes' parents' values mismatch");
            }
        }

        // HWM Ancestor
        if (eq.unequal(first->has_high_water_mark_ancestor(), second->has_high_water_mark_ancestor())) {
            return eq.fail("Has HWM ancestor mismatch");
        }
        if (! eq.pointers_null_agree(first->get_hwm_ancestor(), second->get_hwm_ancestor())) {
            return eq.fail("HWM ancestors don't agree on null state");
        }
        if (first->get_hwm_ancestor() != nullptr) {
            if (eq.unequal(first->get_hwm_ancestor()->get_value(), second->get_hwm_ancestor()->get_value())) {
                return eq.fail("HWM ancestor's value mismatch");
            }
        }

        // Children
        if (eq.unequal(first->get_child_count(), second->get_child_count())) {
            return eq.fail("Node child counts mismatch");
        }
        if (eq.unequal(first->does_own_children(), second->does_own_children())) {
            return eq.fail("Nodes' down_own_children mismatch");
        }
        for (size_t child_id = 0; child_id < first->get_child_count(); child_id++) {
            if (! eq.pointers_null_agree(first->get_child(child_id), second->get_child(child_id))) {
                return eq.fail("Child ID " + to_string_any(child_id) + " mismatch on null state");
            }
            if (first->get_child(child_id) != nullptr) {
                if (eq.unequal(first->get_child(child_id)->get_value(), second->get_child(child_id)->get_value())) {
                    return eq.fail("Child ID " + to_string_any(child_id) + " values mismatch");
                }
                // Ensure each child knows we are the parent.
                if (! eq.same_address(first->get_child(child_id)->get_parent(), first)) {
                    return eq.fail("First's node with Child ID " + to_string_any(child_id) + " doesn't have link back to parent");
                }
                if (! eq.same_address(second->get_child(child_id)->get_parent(), second)) {
                    return eq.fail("Second's node with Child ID " + to_string_any(child_id) + " doesn't have link back to parent");
                }
            }
        }

        // Remaining flags.
        if (eq.unequal(first->is_below_high_water_mark(), second->is_below_high_water_mark())) {
            return eq.fail("Nodes' is_below_high_water_mark mismatch");
        }
        if (eq.unequal(first->is_initialized(), second->is_initialized())) {
            return eq.fail("Nodes' is_initialize mismatch");
        }

        // FG Chain Bits
        if (eq.unequal(first->get_fg_chain_length(), second->get_fg_chain_length())) {
            return eq.fail("Nodes' FG chain length mismatch");
        }
        if (eq.unequal(first->get_fg_chain_string(), second->get_fg_chain_string())) {
            return eq.fail("Nodes' FG chain string mismatch");
        }

        // Guess we made it here.  All good.
        return true;
    }



    /**
    * @brief Compare another node to this one.
    *
    * This is a member helper which simply forwards to `Node::st_equal()`.
    *
    * @param second The second Node to compare against this.
    * @param err Pointer to a string where inequality or error messages are stored.
    * @return True if equal, false otherwise.
    */
    bool equal(const Node<T>* second, std::string* err = nullptr) const {
        return Node<T>::st_equal(this, second, err);
    }



    /**
    * @brief Serialize the node specifics of this object for export.
    *
    * Serialization happens in this order:
    *   1. Value.
    *   2. Parent.
    *   3. HWM Ancestor.
    *   4. Flags.
    *   5. Child count.
    *
    * @note Children are not exported.  They are linked during deserialization.
    * @note This method does not throw.
    * @param out The stream to write data to.
    * @param err Pointer to a string where errors, if any, are written.
    * @return A boolean indicating success or failure.  Do not discard.
    */
    [[nodiscard]] bool serialize(std::ostream& out, std::string* err = nullptr) const {
        StreamHelper sh(nullptr, &out, err);
        sh.set_category("Node");

        // Value
        if (! sh.serialize_integral(_value)) {
            return sh.fail(" _value==" + to_string_any(_value));
        }

        // Parent
        if (_parent == nullptr) {
            if (! sh.serialize_integral(T(0))) {
                return sh.fail("no parent (aka: T(0))");
            }
        } else {
            if (! sh.serialize_integral(_parent->get_value())) {
                return sh.fail("_parent->get_value()==" + to_string_any(_parent->get_value()));
            }
        }

        // HWM Ancestor
        if (_hwm_ancestor == nullptr) {
            if (! sh.serialize_integral(T(0))) {
                return sh.fail("no hwm_ancestor (aka: T(0))");
            }
        } else {
            if (! sh.serialize_integral(_hwm_ancestor->get_value())) {
                return sh.fail("_hwm_ancestor->get_value()==" + to_string_any(_hwm_ancestor->get_value()));
            }
        }

        // Children
        // We don't actually need these.  We rebuild (deserialize) from the parents down to the children, therefore reconstruction
        // is merely adding a node, linking to the parent value (which we wrote above), and then telling that parent to own "this".

        // Flags
        bool b_is_below_hwm = _is_below_hwm;
        bool b_has_hwm_ancestor = _has_hwm_ancestor;
        bool b_is_initialized = _is_initialized;
        bool b_owns_children = _owns_children;
        if (! sh.serialize_bool(b_is_below_hwm)) {
            return sh.fail("_is_below_hwm==" + std::to_string(b_is_below_hwm));
        }
        if (! sh.serialize_bool(b_has_hwm_ancestor)) {
            return sh.fail("_has_hwm_ancestor==" + std::to_string(b_has_hwm_ancestor));
        }
        if (! sh.serialize_bool(b_is_initialized)) {
            return sh.fail("_is_initialized==" + std::to_string(b_is_initialized));
        }
        if (! sh.serialize_bool(b_owns_children)) {
            return sh.fail("_owns_children==" + std::to_string(b_owns_children));
        }

        // Child Count
        if (! sh.serialize_integral(_child_count)) {
            return sh.fail("_child_count==" + to_string_any(_child_count));
        }

        // All Good
        return true;
    }



    /**
    * @brief Deserialize this object for import following a previous `serialize()`.
    *
    * Deserialization happens in the same order as serialization, obviously.
    *
    * @note This method does not throw.
    * @param in The stream to read data from.
    * @param parent_v A reference to return parent value to (for linking/validation).  Zero == null when serialized.
    * @param hwm_ancestor_v A reference to return HWM ancestor value to (for linking/validation).  Zero == null when serialized.
    * @param child_count A reference to return child count to (for linking/validation).
    * @param err Pointer to a string where errors, if any, are written.
    * @return A boolean indicating success or failure.  Do not discard.
    */
    [[nodiscard]] bool deserialize(std::istream& in, T& parent_v, T& hwm_ancestor_v, uint8_t& child_count, std::string* err = nullptr) {
        StreamHelper sh(&in, nullptr, err);
        sh.set_category("Node");

        // Reset
        reset();

        // Value
        if (! sh.deserialize_integral(_value)) {
            return sh.fail("couldn't read _value");
        }

        // Parent
        if (! sh.deserialize_integral(parent_v)) {
            return sh.fail("couldn't read _parent's value");
        }

        // HWM Ancestor
        if (! sh.deserialize_integral(hwm_ancestor_v)) {
            return sh.fail("couldn't read hwm ancestor's value");
        }

        // Flags
        // We use bit fields, which cannot bind to bool&.  So we need a tmp.
        bool b_tmp;
        if (! sh.deserialize_bool(b_tmp)) {
            return sh.fail("couldn't read _is_below_hwm");
        }
        _is_below_hwm = b_tmp;
        if (! sh.deserialize_bool(b_tmp)) {
            return sh.fail("couldn't read _has_hwm_ancestor");
        }
        _has_hwm_ancestor = b_tmp;
        if (! sh.deserialize_bool(b_tmp)) {
            return sh.fail("couldn't read _is_initialized");
        }
        _is_initialized = b_tmp;
        if (! sh.deserialize_bool(b_tmp)) {
            return sh.fail("couldn't read _owns_children");
        }
        _owns_children = b_tmp;

        // Child count is returned, because deserializing a node doesn't actually make the kids.
        if (! sh.deserialize_integral(child_count)) {
            return sh.fail("couldn't read child_count");
        }
        _child_count = 0;

        // All good
        return true;
    }

};
