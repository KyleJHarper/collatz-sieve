#pragma once

#include "concepts.hpp"
#include "node.hpp"
#include <stdexcept>
#include "binary_tree_math.hpp"
#include "binary_tree_coverage.hpp"
#include "binary_tree_options.hpp"
#include "binary_tree_types.hpp"
#include "node_bitmap.hpp"
#include "node_bitmap_traits.hpp"
#include "equality_helper.hpp"
#include "stream_helper.hpp"
#include <tbb/parallel_sort.h>




/**
* @class BinaryTreeImplicitImpl
* @brief A mathematically equivalent implimentation of a materialized `BinaryTree`.
*
* A mathematically equivalent analog to the `BinaryTreeMaterializedImpl` approach.  Nodes objects and other values are calculated
* on-the-fly instead of being stored in RAM.
*
* Specifically, instead of a level map of all surviving nodes and a vector of pointers to them for each level, this implementation
* computes the values, detect coverage (HWM), and update a roaring bitmap.  Tracking and iteration become implicit, hence this
* implementation's namesake.
*
* This implementation is vastly superior in almost every metric, and is the default as of version 4.0.0.
*
* @tparam T Any supported integral (see concepts.hpp).
*/
template<AnySupportedIntegral T>
class BinaryTreeImplicitImpl {
    private:
    /// @brief The root of a tree.  Not navigable in an implicit tree.
    /// @note The main purpose of this node nowadays is knowing whether the tree was rooted at 0 or 1.
    Node<T> *_root_node = nullptr;
    /// @brief Number of levels in the tree.
    level_t _level_count = 0;
    /// @brief Bitmap containing a list of all positions still not covered.
    NodeBitmap<T> _uncovered_positions;
    /// @brief Map of levels and their associated coverage.
    std::unordered_map<level_t, BinaryTreeCoverage<T>> _coverage_map;
    /// @brief Collection of ancestors, which are High-Water Mark nodes that originally truncated a subtree.
    std::vector<Node<T>*> _ancestors;
    /// @brief Flag to determine if this tree is initialized, meaning it's had at least one level added.
    bool _is_initialized = false;
    /// @brief Flag to determine if HWM ancestors should be added to our vector or simply discarded.
    bool _is_preserving_ancestors = BinaryTreeOptions{}.preserve_ancestors;
    /// @brief Flag to determine if verification of non-HWM nodes should happen.  See `BinaryTree` or CHANGELOG 3.4.0 for details.
    bool _is_verifying_non_hwm_nodes = BinaryTreeOptions{}.verify_non_hwm_nodes;
    /// @brief Track the type of this tree for external comparison later.
    const TreeTypeEnum _tree_type = TreeTypeEnum::IMPLICIT;



    public:
    /// @name Lifecycle Management
    /// @{

    /**
    * @brief Default constructor.
    */
    BinaryTreeImplicitImpl() = default;



    /**
    * @brief Destructor will remove any ancestors accumulated, and that's all.
    */
    ~BinaryTreeImplicitImpl() {
        for (Node<T>* node : _ancestors) {
            delete node;
        }
        _ancestors.clear();
    }



    /**
    * @brief Reset members to make this act like a new object.  Mostly for `init()`.
    */
    void reset() {
        _is_initialized = false;
        _coverage_map.clear();
        _uncovered_positions.clear();
        _level_count = 0;
        _ancestors.clear();
        _root_node = nullptr;
        _is_verifying_non_hwm_nodes = BinaryTreeOptions{}.verify_non_hwm_nodes;
        _is_preserving_ancestors = BinaryTreeOptions{}.preserve_ancestors;
    }



    /**
    * @brief Initializes the object, calling `reset()` if already initialized.
    * @param levels Number of levels to build this tree, inclusive.
    * @param opts Options to control tree-buiding behavior.  Note that not all apply to implicit trees, such as pruning.
    */
    void init(level_t levels, const BinaryTreeOptions opts = BinaryTreeOptions{}) {
        // Reset object if necessary.
        if(_is_initialized) { reset(); }
        _is_initialized = true;
        // Set options.
        _is_verifying_non_hwm_nodes = opts.verify_non_hwm_nodes;
        _is_preserving_ancestors = opts.preserve_ancestors;
        // Now add the other levels, if needed.
        while (_level_count < levels) {
            this->add_level();
        }
    }

    /// @}



    /// @name Accessors Common to All Trees
    /// @{

    /// @brief Get the tree type of this instance.
    TreeTypeEnum get_tree_type() const { return _tree_type; }

    /// @brief Get the level count.
    level_t get_level_count() const { return _level_count; }
    /// @brief Set the level count explicitly.
    /// @warning This is intended for serialization/deserilization only.
    /// @param level_count The level count to set.
    void set_level_count(level_t level_count) { _level_count = level_count; }

    /// @brief Get a readonly pointer to the root node.
    Node<T>* get_root_node() const { return _root_node; }
    /// @brief Get a read-write reference to the pointer to the root node.
    /// @note You get a ref to a pointer to allow manipulation of this member directly.
    /// @warning This is intended for serialization/deserilization only.
    Node<T>*& get_root_node_rw() { return _root_node; }

    /// @brief Get a readonly reference to the coverage map.
    const std::unordered_map<level_t, BinaryTreeCoverage<T>>& get_coverage_map() const { return _coverage_map; }
    /// @brief Get a read-write reference to the coverage map.
    /// @warning This is intended for serialization/deserilization only.
    std::unordered_map<level_t, BinaryTreeCoverage<T>>& get_coverage_map_rw() { return _coverage_map; }

    /// @brief Get a readonly reference to the ancestors.
    const std::vector<Node<T>*>& get_ancestors() const { return _ancestors; }
    /// @brief Get a read-write reference to the ancestors.
    /// @warning This is intended for serialization/deserilization only.
    std::vector<Node<T>*>& get_ancestors_rw() { return _ancestors; }

    /// @brief Get a flag whether this tree is preserving ancestors.
    bool is_preserving_ancestors() const { return _is_preserving_ancestors; }
    /// @brief Change the flag for whether this tree is preserving ancestors.
    /// @warning This is intended for serialization/deserilization only.
    /// @param value The true or false value to set.
    void set_is_preserving_ancestors(bool value) { _is_preserving_ancestors = value; }

    /// @brief Get a flag whether this tree is verifying non-HWM nodes.
    bool is_verifying_non_hwm_nodes() const { return _is_verifying_non_hwm_nodes; }
    /// @brief Explicitly disable non-HWM node verification.
    void disable_non_hwm_node_verification() { _is_verifying_non_hwm_nodes = false; }
    /// @brief Explicitly enable non-HWM node verification.
    void enable_non_hwm_node_verification() { _is_verifying_non_hwm_nodes = true; }

    /// @brief Get a flag whether this tree is initialized.
    bool is_initialized() const { return _is_initialized; }
    /// @brief Change the flag for whether this tree is initialized.
    /// @warning This is intended for serialization/deserilization only.
    /// @param value The true or false value to set.
    void set_is_initialized(bool value) { _is_initialized = value; }

    /// @}

    /// @name Implicit-Only Accessors
    /// @{

    /// @brief Responds true to `is_implicit()`, mostly for the `BinaryTree` facade.
    bool is_implicit() const { return true; }
    /// @brief Get a reference to the uncovered positions, which are the node positions not covered by a High-Water Mark ancestor.
    /// @note These are left-to-right positions, not node values.
    const NodeBitmap<T>& get_uncovered_positions() const { return _uncovered_positions; }

    /// @}



    /**
    * @brief Deeply scan the object and report its size.
    */
    size_t deep_size() const {
        size_t total = sizeof(*this);

        // Account for _uncovered_positions.
        total += _uncovered_positions.deep_size();

        // Account for _coverage_map
        total += sizeof(_coverage_map);
        total += _coverage_map.bucket_count() * sizeof(void*);
        for (const auto& [level, coverage] : _coverage_map) {
            total += sizeof(level);
            total += sizeof(coverage);
        }

        return total;
    }



    /**
    * @brief Compare two implicit trees' specific internals and return true if identical.
    *
    * This method only tests implicit tree specifics.  Most equality happens via the `BinaryTree` facade since they share common
    * members between implementations (e.g.: level count).
    *
    * This function checks:
    *   1. The uncovered positions (`NodeBitmap`) are equal.
    *
    * @param first The first tree to compare.
    * @param second The second tree to compare.
    * @param err Pointer to a string where inequality or error messages are stored.
    * @return True if equal, false otherwise.
    */
    static bool st_equal(const BinaryTreeImplicitImpl<T>& first, const BinaryTreeImplicitImpl<T>& second, std::string* err = nullptr) {
        EqualityHelper eq(err);
        eq.set_category("BinaryTreeImplicitImpl");

        // Uncovered Positions NodeBitmap
        if (! NodeBitmap<T>::st_equal(first.get_uncovered_positions(), second.get_uncovered_positions(), err)) {
            return eq.fail("Node bitmap issues");
        }

        // All good.
        return true;
    }



    /**
    * @brief Compare another implicit tree to this one.
    *
    * This is a member helper which simply forwards to `BinaryTreeImplicitImpl::st_equal()`.
    *
    * @param second The second tree to compare against this.
    * @param err Pointer to a string where inequality or error messages are stored.
    * @return True if equal, false otherwise.
    */
    bool equal(const BinaryTreeImplicitImpl<T>& second, std::string* err = nullptr) const {
        return BinaryTreeImplicitImpl<T>::st_equal(*this, second, err);
    }



    /**
    * @brief Serialize the implicit tree specifics of this object for export.
    *
    * Serialization happens in this order:
    *   1. Uncovered positions (`NodeBitmap`)
    *
    * @note The `BinaryTree` facade handles serialization of common items between implementations.
    * @note This method does not throw.
    * @param out The stream to write data to.
    * @param err Pointer to a string where errors, if any, are written.
    * @return A boolean indicating success or failure.  Do not discard.
    */
    [[nodiscard]] bool serialize(std::ostream& out, std::string* err = nullptr) const {
        StreamHelper sh(nullptr, &out, err);
        sh.set_category("BinaryTreeImplicitImpl");

        // No unique properties.

        // Uncovered Positions NodeBitmap
        if (! _uncovered_positions.serialize(out, err)) {
            return sh.fail("_uncovered_positions error");
        }

        // All good.
        return true;
    }



    /**
    * @brief Deserialize this object for import following a previous `serialize()`.
    *
    * Deserialization happens in the same order as serialization, obviously.
    *
    * @note The `BinaryTree` facade handles deserialization of common items between implementations.
    * @note This method does not throw.
    * @param in The stream to read data from.
    * @param err Pointer to a string where errors, if any, are written.
    * @return A boolean indicating success or failure.  Do not discard.
    */
    [[nodiscard]] bool deserialize(std::istream& in, std::string* err = nullptr) {
        StreamHelper sh(&in, nullptr, err);
        sh.set_category("BinaryTreeImplicitImpl");

        // No unique properties.

        // Uncovered positions NodeBitmap.
        if (! _uncovered_positions.deserialize(in, err)) {
            return sh.fail("unable to deserialize the uncovered positions");
        }

        // All good.
        return true;
    }



    /**
    * @struct AddLevelTLS
    * @brief A thread-local cache structure for `add_level()`.
    *
    * The `NodeBitmap` has a multithreading (OMP) for-each iterator which we leverage to engage multiple CPUs when adding nodes to
    * a level.  This struct creates safety and scratch space for that.
    *
    * @warning This is not meant to be used anywhere else.
    */
    struct AddLevelTLS {
        /// @brief Dedicated `NodeBitmap` for the thread to use without guards.  Is merged later.
        NodeBitmap<T> uncovered_bitmap;
        /// @brief Dedicated pool of newly-found ancestors (HWM nodes).
        std::vector<Node<T>*> new_ancestors;
        /// @brief Left child's value (scratch space).
        T left_value;
        /// @brief Left child's position (scratch space).
        T left_position;
        /// @brief Left child `Node` object (scratch space).  Reused via `Node::init()`.
        Node<T> left_node;
        /// @brief Right child's value (scratch space).
        T right_value;
        /// @brief Right child's position (scratch space).
        T right_position;
        /// @brief Right child `Node` object (scratch space).  Reused via `Node::init()`.
        Node<T> right_node;
    };



    /**
    * @brief Add a level to the tree.
    *
    * Implicit tree building happens by tracking the left-to-right positions (not values) of nodes which haven't been removed by a
    * HWM node or ancestor.  This is representatively identical to a materialized tree's last level of nodes: anything remaining in
    * the `_uncovered_positions` are the parent nodes from the previous level whose children must be processed.
    *
    * To achieve any sort of threading performance, each thread will get thread-local storage (TLS) from an `AddLevelTLS` struct,
    * which includes its own `NodeBitmap` and vector of ancestors (if tracking them).  When all threading is finished, the results
    * are merged into `_uncovered_positions`, coverage is calculated via simple arithmetic, and this function ends.
    *
    * Threading itself is handed off to `NodeBitmap::for_each_transformer()`, along with the aforementioned callback (TLS) storage.
    * This is in contrast to the `BinaryTreeMaterializedImpl` which handles OMP directly inside the its own add level method.
    */
    void add_level() {
        // When there are no levels, this simply crafts a 0- or 1-based root node and manually sets level map and coverage.
        if (_level_count == 0) {
            _level_count = 1;
            _root_node = new Node<T>(BinaryTreeMath<T>::get_root_value());
            _coverage_map[_level_count].set_covered(0);
            _coverage_map[_level_count].set_total(1);
            _uncovered_positions.add(1);
            return;
        }

        // Bump level count.
        _level_count++;

        // Establish TLS for our callback.
        std::vector<AddLevelTLS> callback_storage;
        // Track our own local High-Water Mark.  Since nodes are not sequentially ordered, we can only set this to first node - 1.
        T add_level_high_water_mark = BinaryTreeMath<T>::st_first_node_value_of_level(_level_count) - 1;
        if (add_level_high_water_mark < 1) {
            add_level_high_water_mark = 1;
        }

        // Now loop.
        _uncovered_positions.for_each_transformer(BitmapTransformerPolicy::PARALLEL, callback_storage, [&](const T& cb_position, AddLevelTLS& tls) {
            // Scale the uncovered position to this level.  Make these TLS storage to avoid alloc() on GMP path.
            if constexpr(BuiltinIntegral<T>) {
                tls.left_position = (cb_position << 1) - 1;
                tls.right_position = cb_position << 1;
            } else {
                mpz_mul_2exp(tls.left_position.get_mpz_t(), cb_position.get_mpz_t(), 1);
                mpz_sub_ui(tls.left_position.get_mpz_t(), tls.left_position.get_mpz_t(), 1);
                mpz_mul_2exp(tls.right_position.get_mpz_t(), cb_position.get_mpz_t(), 1);
            }

            // Unroll this loop manually.
            // for (const T& position : tls.positions) {...}

            // Calculate the values from positions.  Use an out param on GMP path.
            if constexpr(BuiltinIntegral<T>) {
                tls.left_value = BinaryTreeMath<T>::st_node_value_by_position_and_level(tls.left_position, _level_count);
                tls.right_value = BinaryTreeMath<T>::st_node_value_by_position_and_level(tls.right_position, _level_count);
            } else {
                BinaryTreeMath<T>::st_node_value_by_position_and_level(tls.left_position, _level_count, tls.left_value);
                BinaryTreeMath<T>::st_node_value_by_position_and_level(tls.right_position, _level_count, tls.right_value);
            }

            // Initialize the node and test HWM for ancestry.
            tls.left_node.init(tls.left_value);
            tls.right_node.init(tls.right_value);
            if (_is_preserving_ancestors) {
                if (tls.left_node.is_below_high_water_mark()) {
                    tls.new_ancestors.push_back(new Node<T>(tls.left_value));
                }
                if (tls.right_node.is_below_high_water_mark()) {
                    tls.new_ancestors.push_back(new Node<T>(tls.right_value));
                }
            }

            // Perform verification, if requested.
            if (_is_verifying_non_hwm_nodes) {
                if (tls.left_node.is_below_high_water_mark() == false) {
                    if (Collatz<T>::st_verify(tls.left_value, add_level_high_water_mark) == false) {
                        throw std::logic_error("Node value " + to_string_any(tls.left_value) + " didn't verify.  How?");
                    }
                }
                if (tls.right_node.is_below_high_water_mark() == false) {
                    if (Collatz<T>::st_verify(tls.right_value, add_level_high_water_mark) == false) {
                        throw std::logic_error("Node value " + to_string_any(tls.right_value) + " didn't verify.  How?");
                    }
                }
            }

            // Write the POSITION to the TLS bitmap if it didn't hit HWM.
            if (tls.left_node.is_below_high_water_mark() == false) {
                tls.uncovered_bitmap.add(tls.left_position);
            }
            if (tls.right_node.is_below_high_water_mark() == false) {
                tls.uncovered_bitmap.add(tls.right_position);
            }

            // Return false.  There is no stopping condition.
            return false;
        });

        // Merge TLS data into the tree's trackers.
        _uncovered_positions.clear();
        for (const AddLevelTLS& storage : callback_storage) {
            _uncovered_positions |= storage.uncovered_bitmap;
            _ancestors.insert(_ancestors.end(), storage.new_ancestors.begin(), storage.new_ancestors.end());
        }

        // Always sort ancestors.
        if (_is_preserving_ancestors) {
            tbb::parallel_sort(_ancestors.begin(), _ancestors.end(), [](const Node<T>* a, const Node<T>* b) {
                if constexpr(BuiltinIntegral<T>) {
                    return a->get_value() < b->get_value();
                } else {
                    return mpz_cmp(a->get_value().get_mpz_t(), b->get_value().get_mpz_t()) < 0;
                }
            });
        }

        // Persist the coverage to the new level.  It's just a sum of the uncovered size() values subtracted from the node total.
        T total = BinaryTreeMath<T>::st_node_count_of_level(_level_count);
        T uncovered = _uncovered_positions.cardinality();
        _coverage_map[_level_count] = BinaryTreeCoverage<T>(total - uncovered, total);

        // Invoke optimize above level 32.
        if (_level_count > 32) {
            _uncovered_positions.optimize();
        }
    }


};
