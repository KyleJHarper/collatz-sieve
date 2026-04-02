#pragma once

#include "binary_tree_backend_interface.hpp"
#include "concepts.hpp"
#include <omp.h>
#include <tbb/parallel_sort.h>
#include "node_bitmap.hpp"
#include "node_bitmap_traits.hpp"





//
// A mathematically equivalent analog to the BinaryTreeMaterialized class above.  Nodes objects and other values are calculated
// more on-the-fly instead of being stored in RAM.
//
// Specifically, instead of a level map of all surviving nodes and a vector of pointers to ancestors, we only track the ancestors
// and then leverage their level L and position P properties to calculate coverage, iterate over survivors, etc.
//
template<AnySupportedIntegral T>
class BinaryTreeImplicit : public IBinaryTreeBackend<T> {
    private:
    Node<T> *_root_node = nullptr;
    size_t _level_count = 0;
    NodeBitmap<T> _uncovered_positions;
    std::unordered_map<size_t, BinaryTreeCoverage<T>> _coverage_map;
    std::vector<Node<T>*> _ancestors;
    bool _is_initialized = false;
    bool _preserve_ancestors = false;
    bool _is_verifying_non_hwm_nodes = false;

    public:
    //
    // Constructors
    //
    static constexpr BinaryTreeOptions DEFAULT_OPTS{};
    BinaryTreeImplicit(const BinaryTreeOptions& opts = DEFAULT_OPTS) {
        _is_verifying_non_hwm_nodes = opts.verify_non_hwm_nodes;
        _preserve_ancestors = opts.preserve_ancestors;
    }



    //
    // Destructor
    //
    ~BinaryTreeImplicit() {
        for (Node<T>* node : _ancestors) {
            delete node;
        }
        _ancestors.clear();
    }



    //
    // Initialize
    // Builds the object, reusing it if necessary.
    //
    void init(size_t levels) override {
        // Sanity check.  T must support requested tree size.
        this->assert_level_will_fit(levels);
        // Reset object if necessary.
        if(_is_initialized) { reset(); }
        _is_initialized = true;
        // Build the first level's root node, supporting both 0- and 1-based trees.
        _level_count = 1;
        _root_node = new Node<T>(BinaryTreeMath<T>::get_root_value());
        _coverage_map[_level_count].set_covered(0);
        _coverage_map[_level_count].set_total(1);
        _uncovered_positions.add(1);
        // Now add the other levels, if needed.
        while (_level_count < levels) {
            this->add_level();
        }
    }



    //
    // Reset Object
    // Reset members to make this act like a new() object.
    //
    void reset() override {
        _is_initialized = false;
        _coverage_map.clear();
        _uncovered_positions.clear();
        _level_count = 0;
        _ancestors.clear();
        _is_verifying_non_hwm_nodes = BinaryTreeOptions{}.verify_non_hwm_nodes;
        _preserve_ancestors = BinaryTreeOptions{}.preserve_ancestors;
    }



    //
    // Getters and Accessors
    //
    size_t get_level_count() const override { return _level_count; }
    Node<T>* get_root_node() const override { return _root_node; }
    const NodeBitmap<T>& get_uncovered_positions() const override { return _uncovered_positions; }
    const std::unordered_map<size_t, BinaryTreeCoverage<T>>& get_coverage_map() const override { return _coverage_map; }
    const std::vector<Node<T>*> get_ancestors() const override { return _ancestors; }
    const std::unordered_map<size_t, std::vector<Node<T>*>>& get_level_map() const override {
        throw std::logic_error("Implicit trees do not have a level map");
    }
    bool is_verifying_non_hwm_nodes() const override { return _is_verifying_non_hwm_nodes; }
    void disable_non_hwm_node_verification() { _is_verifying_non_hwm_nodes = false; }
    void enable_non_hwm_node_verification() { _is_verifying_non_hwm_nodes = true; }
    bool is_pruning_hwm_nodes() const override {
        throw std::logic_error("Implicit trees do not have an is_pruning_hwm_nodes property.");
    }
    bool is_pruning_parent_levels() const override {
        throw std::logic_error("Implicit trees do not have an is_pruning_parent_levels property.");
    }


    //
    // Node Count
    // Returns the mathematically correct number of nodes as if they existed in RAM.
    //
    T node_count() const override {
        return BinaryTreeMath<T>::st_node_count_of_tree(_level_count);
    }



    //
    // Object Size
    // Deeply scan the object and report its size.  Since it's implicit, this is simpler.
    //
    size_t deep_size() const override {
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
    // Add Level
    // Add a level to the tree, which simply means scan the next level for new covered sections (HWM ancestors).
    //
    struct AddLevelTLS {
        NodeBitmap<T> uncovered_bitmap;
        std::vector<Node<T>*> new_ancestors;
        T value;
        T positions[2];
        Node<T> node;
    };
    void add_level() override {
        // Confirm the new level will fit, warn about verification, and then bump the count.
        this->assert_level_will_fit(_level_count + 1);
        this->assert_level_verification(_level_count + 1, _is_verifying_non_hwm_nodes);
        _level_count++;

        // Establish TLS for our callback.
        std::vector<AddLevelTLS> callback_storage;
        // Track our own local High-Water Mark.  Since nodes are not sequentially ordered, we can only set this to first node - 1.
        const T add_level_high_water_mark = BinaryTreeMath<T>::st_first_node_of_level(_level_count) - 1;
        // Build a few constexpr for the policy, stop flag, etc., which won't change at runtime.
        constexpr BitmapTransformerPolicy parallel_policy = BitmapTransformerPolicy::PARALLEL;
        constexpr bool stop = false;

        // Now loop.
        _uncovered_positions.for_each_transformer(parallel_policy, callback_storage, [&](const T& value, AddLevelTLS& tls) {
            // Scale the uncovered position to this level.  Make these TLS storage to avoid alloc() on GMP path.
            if constexpr(BuiltinIntegral<T>) {
                tls.positions[0] = (value << 1) - 1;  // Left child.
                tls.positions[1] = value << 1;        // Right child.
            } else {
                mpz_mul_2exp(tls.positions[0].get_mpz_t(), value.get_mpz_t(), 1);
                mpz_sub_ui(tls.positions[0].get_mpz_t(), tls.positions[0].get_mpz_t(), 1);
                mpz_mul_2exp(tls.positions[1].get_mpz_t(), value.get_mpz_t(), 1);
            }

            for (const T& position : tls.positions) {
                // Calculate the value from position.  Use an out param on GMP path.
                if constexpr(BuiltinIntegral<T>) {
                    tls.value = BinaryTreeMath<T>::st_node_value_by_position_and_level(position, _level_count);
                } else {
                    BinaryTreeMath<T>::st_node_value_by_position_and_level(position, _level_count, tls.value);
                }

                // Initialize the node and test HWM for ancestry.
                tls.node.init(tls.value);
                if (_preserve_ancestors && tls.node.is_below_high_water_mark()) {
                    tls.new_ancestors.push_back(new Node<T>(tls.value));
                }

                // Perform verification, if requested.
                if (_is_verifying_non_hwm_nodes) {
                    if (tls.node.is_below_high_water_mark() == false) {
                        if (Collatz<T>::st_verify(tls.value, add_level_high_water_mark) == false) {
                            throw std::logic_error("Node value " + to_string_any(tls.value) + " didn't verify.  How?");
                        }
                    }
                }

                // Write the POSITION to the TLS bitmap if it didn't hit HWM.
                if (tls.node.is_below_high_water_mark() == false) {
                    tls.uncovered_bitmap.add(position);
                }
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

        // Persist the coverage to our new level.  It's just a sum of the uncovered size() values subtracted from the node total.
        T total = BinaryTreeMath<T>::st_node_count_of_level(_level_count);
        T uncovered = _uncovered_positions.cardinality();
        _coverage_map[_level_count] = BinaryTreeCoverage<T>(total - uncovered, total);
    }

};
