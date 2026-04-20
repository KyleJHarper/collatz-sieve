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




//
// A mathematically equivalent analog to the BinaryTreeMaterialized class above.  Nodes objects and other values are calculated
// more on-the-fly instead of being stored in RAM.
//
// Specifically, instead of a level map of all surviving nodes and a vector of pointers to ancestors, we only track the ancestors
// and then leverage their level L and position P properties to calculate coverage, iterate over survivors, etc.
//
template<AnySupportedIntegral T>
class BinaryTreeImplicitImpl {
    private:
    Node<T> *_root_node = nullptr;
    size_t _level_count = 0;
    NodeBitmap<T> _uncovered_positions;
    std::unordered_map<size_t, BinaryTreeCoverage<T>> _coverage_map;
    std::vector<Node<T>*> _ancestors;
    bool _is_initialized = false;
    bool _is_preserving_ancestors = BinaryTreeOptions{}.preserve_ancestors;
    bool _is_verifying_non_hwm_nodes = BinaryTreeOptions{}.verify_non_hwm_nodes;
    const TreeTypeEnum _tree_type = TreeTypeEnum::IMPLICIT;
    static constexpr BinaryTreeOptions DEFAULT_OPTS{};



    public:
    //
    // Constructors
    //
    BinaryTreeImplicitImpl() = default;



    //
    // Destructor
    //
    ~BinaryTreeImplicitImpl() {
        for (Node<T>* node : _ancestors) {
            delete node;
        }
        _ancestors.clear();
    }



    //
    // Initialize
    // Builds the object, reusing it if necessary.
    //
    void init(size_t levels, const BinaryTreeOptions opts = DEFAULT_OPTS) {
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



    //
    // Reset Object
    // Reset members to make this act like a new() object.
    //
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



    //
    // Getters and Accessors
    //
    // Common properties.
    TreeTypeEnum get_tree_type() const { return _tree_type; }
    size_t get_level_count() const { return _level_count; }
    void set_level_count(size_t level_count) { _level_count = level_count; }
    Node<T>* get_root_node() const { return _root_node; }
    Node<T>*& get_root_node_rw() { return _root_node; }
    const std::unordered_map<size_t, BinaryTreeCoverage<T>>& get_coverage_map() const { return _coverage_map; }
    std::unordered_map<size_t, BinaryTreeCoverage<T>>& get_coverage_map_rw() { return _coverage_map; }
    const std::vector<Node<T>*>& get_ancestors() const { return _ancestors; }
    std::vector<Node<T>*>& get_ancestors_rw() { return _ancestors; }
    bool is_preserving_ancestors() const { return _is_preserving_ancestors; }
    void set_is_preserving_ancestors(bool value) { _is_preserving_ancestors = value; }
    bool is_verifying_non_hwm_nodes() const { return _is_verifying_non_hwm_nodes; }
    void disable_non_hwm_node_verification() { _is_verifying_non_hwm_nodes = false; }
    void enable_non_hwm_node_verification() { _is_verifying_non_hwm_nodes = true; }
    bool is_initialized() const { return _is_initialized; }
    void set_is_initialized(bool value) { _is_initialized = value; }
    //
    // Implicit-Specific
    bool is_implicit() const { return true; }
    const NodeBitmap<T>& get_uncovered_positions() const { return _uncovered_positions; }



    //
    // Object Size
    // Deeply scan the object and report its size.  Since it's implicit, this is simpler.
    //
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



    //
    // Equal
    // Compare all of the members and (meta) data.
    //
    // Returns true if they are equal in representation.  False otherwise.
    // Will explain what failed to *err if sent.
    //
    // Only implementation-specific checks go here.  Facade handles common.
    static bool st_equal(const BinaryTreeImplicitImpl<T>& first, const BinaryTreeImplicitImpl<T>& second, std::string* err = nullptr) {
        EqualityHelper eq(err);
        eq.set_category("BinaryTreeImplicitImpl");

        // Uncovered Positions NodeBitmap
        if (! NodeBitmap<T>::st_equal(first.get_uncovered_positions(), second.get_uncovered_positions(), err)) {
            return eq.fail("Node bitmap issues");
        }

        // Guess we made it here.  All good.
        return true;
    }
    //
    // Member helper.
    bool equal(const BinaryTreeImplicitImpl<T>& second, std::string* err = nullptr) const {
        return BinaryTreeImplicitImpl<T>::st_equal(*this, second, err);
    }



    //
    // Serialize
    // Serializes the implementation-specific details.  Facade handles common.
    //
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



    //
    // Deserialize
    // Read data from "in" and reconstruct this object.
    //
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



    //
    // Add Level
    // Add a level to the tree, which simply means scan the next level for new covered sections (HWM ancestors).
    //
    struct AddLevelTLS {
        NodeBitmap<T> uncovered_bitmap;
        std::vector<Node<T>*> new_ancestors;
        T left_value;
        T left_position;
        Node<T> left_node;
        T right_value;
        T right_position;
        Node<T> right_node;
    };
    void add_level() {
        // When we have no levels, we'll simply craft a 0- or 1-based root node and manually set level map and coverage.
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
        T add_level_high_water_mark = BinaryTreeMath<T>::st_first_node_of_level(_level_count) - 1;
        if (add_level_high_water_mark < 1) {
            add_level_high_water_mark = 1;
        }

        // Build a few constexpr for the policy, stop flag, etc., which won't change at runtime.
        constexpr bool stop = false;

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

            // Return false.  We have no stopping condition.
            return stop;
        });

        // Merge TLS data into our tree's trackers.
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


        // Persist the coverage to our new level.  It's just a sum of the uncovered size() values subtracted from the node total.
        T total = BinaryTreeMath<T>::st_node_count_of_level(_level_count);
        T uncovered = _uncovered_positions.cardinality();
        _coverage_map[_level_count] = BinaryTreeCoverage<T>(total - uncovered, total);

        // Invoke optimize above level 32.
        if (_level_count > 32) {
            _uncovered_positions.optimize();
        }
    }


};
