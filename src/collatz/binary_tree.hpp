#pragma once

#include <cmath>
#include <cstddef>
#include <gmp.h>
#include <gmpxx.h>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <omp.h>
#include "collatz.hpp"
#include "node.hpp"
#include "binary_tree_coverage.hpp"


//
// Options Package for BinaryTree
//
struct BinaryTreeOptions {
    bool track_node_metadata = false;
    bool prune_hwm_nodes = false;
    bool prune_parent_levels = false;
    bool preserve_ancestors = false;
};


//
// A perfect binary tree mapped to powers of two.  This creates a uniform distribution of nodes in
// the N+/Z space (positive integers), which Collatz is concerned.
//
// When pruning is enabled, we will remove any nodes hitting "hwm", leaving only nodes that still
// need to be tested.
//
template<IntegralOrMPZClass T>
class BinaryTree {
    private:
    Node<T> *_root_node = nullptr;
    size_t _level_count = 0;
    std::unordered_map<size_t, std::vector<Node<T>*>> _level_map;
    std::unordered_map<size_t, BinaryTreeCoverage<T>> _coverage_map;
    std::vector<Node<T>*> _ancestors;
    bool _is_initialized = false;
    bool _track_node_metadata = false;
    bool _is_pruning_hwm_nodes = false;
    bool _is_pruning_parent_levels = false;
    bool _preserve_ancestors = false;

    public:
    // Constructor and Init
    static constexpr BinaryTreeOptions DEFAULT_OPTS{};
    BinaryTree() {}
    BinaryTree(size_t levels, const BinaryTreeOptions& opts = DEFAULT_OPTS) {
        init(levels, opts);
    }
    void init(size_t levels, const BinaryTreeOptions& opts = DEFAULT_OPTS) {
        // Sanity check.  T must support requested tree size.
        assert_level_will_fit(levels);
        // Reset object if necessary.
        if(_is_initialized) { reset(); }
        _is_initialized = true;
        _track_node_metadata = opts.track_node_metadata;
        _is_pruning_hwm_nodes = opts.prune_hwm_nodes;
        _is_pruning_parent_levels = opts.prune_parent_levels;
        _preserve_ancestors = opts.preserve_ancestors;
        _root_node = new Node<T>(0, _track_node_metadata);
        _level_map[0].resize(1);
        _level_map[0][0] = _root_node;
        _coverage_map[0].set_covered(0);
        for (size_t level = 1; level <= levels; ++level) {
            this->add_level();
        }
    }

    // Reset to make this act like a new() object.
    void reset() {
        _is_initialized = false;
        _coverage_map.clear();
        _level_map.clear();
        _ancestors.clear();
        _track_node_metadata = BinaryTreeOptions{}.track_node_metadata;
        _is_pruning_hwm_nodes = BinaryTreeOptions{}.prune_hwm_nodes;
        _is_pruning_parent_levels = BinaryTreeOptions{}.prune_parent_levels;
        _preserve_ancestors = BinaryTreeOptions{}.preserve_ancestors;
    }

    // Destructor
    ~BinaryTree() {
        if (_is_pruning_hwm_nodes) {
            // We need to scan all levels manually and call delete explicitly.
            for (size_t level = 0; level <= _level_count; level++) {
                for (Node<T>* node : _level_map[level]) {
                    if (node != nullptr) {
                        node->own_children(false);
                        delete node;
                    }
                }
            }
        } else {
            // This will cascade to children because Node.own_children is default true.
            delete _root_node;
        }
    }

    // Accessors and properties.
    size_t get_level_count() const { return _level_count; }
    Node<T>* get_root_node() const { return _root_node; }
    const std::unordered_map<size_t, std::vector<Node<T>*>>& get_level_map() const { return _level_map; }
    const std::unordered_map<size_t, BinaryTreeCoverage<T>>& get_coverage_map() const { return _coverage_map; }
    const std::vector<Node<T>*> get_ancestors() const { return _ancestors; }
    bool is_pruning_hwm_nodes() const { return _is_pruning_hwm_nodes; }
    bool is_pruning_parent_levels() const { return _is_pruning_parent_levels; }
    bool tracking_metadata() const { return _track_node_metadata; }

    // Node Counts
    // Returns the REAL node count in the _level_map.
    T node_count() const {
        // It should be: 2^(max_levels + 1) - 1 (if we count node 0)
        // However, due to pruning, just add up the size() values of each level.
        T total = 0;
        for (size_t level = 1; level <= _level_count; level++) {
            total += _level_map.at(level).size();
        }
        return total;
    }
    T node_count_with_root() const {
        return node_count() + 1;
    }

    // Object Size
    size_t deep_size() const {
        size_t total = sizeof(*this);
        // Account for _level_map
        total += sizeof(_level_map);
        for (const auto& [level, nodes] : _level_map) {
            total += sizeof(level);
            total += sizeof(nodes);
            total += nodes.capacity() * sizeof(Node<T>*);
            for (const Node<T>* node : nodes) {
                if (node) {
                    total += node->deep_size();
                }
            }
        }
        // Account for _coverage_map
        total += sizeof(_coverage_map);
        for (const auto& [level, coverage] : _coverage_map) {
            total += sizeof(level);
            total += sizeof(coverage);
        }
        // Unordered map buckets.
        total += _level_map.bucket_count() * sizeof(void*);
        return total;
    }

    // Add a level to the tree.  We simply take the parent nodes and add two children with a steady step value.  We
    // also calculate the coverage for this level.
    //
    // When WHM pruning is enabled we trim all grandparents who meet HWM.  We need the parents to build the child level
    // currently.  We assume the parent generation was already pruned, and we'll test children after we make them.
    void add_level() {
        // Get the parent and child level IDs.
        size_t parent_level = _level_count;
        size_t child_level = _level_count + 1;
        _level_count++;
        assert_level_will_fit(_level_count);
        // Each level will double the size of the tree, so we can't rely on size_t if we're going to
        // support GMP-size values.  We need to respect T.
        T step;
        if constexpr(std::same_as<T, mpz_class>) {
            mpz_pow_ui(step.get_mpz_t(), CollatzConstants::MPZ_TWO.get_mpz_t(), parent_level);
        } else {
            step = static_cast<T>(1ULL << parent_level);
        }
        // Loop through the parents to build the children.
        // Use size_t because that's more objects than we could ever hold in RAM anyway, and if we're past 2^64, we're
        // obviously using HWM pruning, which means counts will fit in size_t anyway.
        auto& parents = _level_map[parent_level];
        size_t parent_count = parents.size();
        size_t child_count = parent_count * 2;
        size_t covered_or_pruned = 0;
        // Resize the vector.
        _level_map[child_level].resize(child_count);
        // Making these thread-local is a bit of a trick; it makes OMP think they're shared, but the
        // thread-local nature means they stay separated.  Prevents realloc() within the loop.
        static thread_local T child_value_1;
        static thread_local T child_value_2;
        // Guard _ancestors with a mutex.
        std::mutex ancestor_lock;
        #pragma omp parallel for schedule(guided) reduction(+:covered_or_pruned) default(none) shared(parents, _level_map, step, child_level, parent_count, _is_pruning_hwm_nodes, ancestor_lock, _preserve_ancestors, _track_node_metadata)
        for(size_t parent_idx = 0; parent_idx < parent_count; parent_idx++) {
            // Find parent.
            Node<T>* parent = parents[parent_idx];
            // Get the child values.  Avoid alloc() with GMP with arithmetic operators.
            if constexpr(std::same_as<T, mpz_class>) {
                mpz_add(child_value_1.get_mpz_t(), parent->get_value().get_mpz_t(), step.get_mpz_t());
                mpz_add(child_value_2.get_mpz_t(), child_value_1.get_mpz_t(), step.get_mpz_t());
            } else {
                child_value_1 = parent->get_value() + step;
                child_value_2 = child_value_1 + step;
            }
            // Create the children.
            Node<T>* child_1 = new Node<T>(child_value_1, _track_node_metadata, parent);
            Node<T>* child_2 = new Node<T>(child_value_2, _track_node_metadata, parent);
            // Tally them.  Prune them if necessesary.  Otherwise add them to the map.
            bool assign_to_map = true;
            if (_preserve_ancestors && child_1->is_below_high_water_mark() && ! child_1->has_high_water_mark_ancestor()) {
                std::lock_guard<std::mutex> lock(ancestor_lock);
                _ancestors.push_back(new Node<T>(child_1->get_value(), false));
            }
            if (child_1->is_below_high_water_mark() || child_1->has_high_water_mark_ancestor()) {
                covered_or_pruned += 1;
                if (_is_pruning_hwm_nodes) {
                    delete child_1;
                    child_1 = nullptr;
                    assign_to_map = false;
                }
            }
            if (assign_to_map) {
                _level_map[child_level][2 * parent_idx] = child_1;
                if (! _is_pruning_hwm_nodes && ! _is_pruning_parent_levels) {
                    parent->assign_child(child_1);
                }
            }
            assign_to_map = true;
            if (_preserve_ancestors && child_2->is_below_high_water_mark() && ! child_2->has_high_water_mark_ancestor()) {
                std::lock_guard<std::mutex> lock(ancestor_lock);
                _ancestors.push_back(new Node<T>(child_2->get_value(), false));
            }
            if (child_2->is_below_high_water_mark() || child_2->has_high_water_mark_ancestor()) {
                covered_or_pruned += 1;
                if (_is_pruning_hwm_nodes) {
                    delete child_2;
                    child_2 = nullptr;
                    assign_to_map = false;
                }
            }
            if (assign_to_map) {
                _level_map[child_level][2 * parent_idx + 1] = child_2;
                if (! _is_pruning_hwm_nodes && ! _is_pruning_parent_levels) {
                    parent->assign_child(child_2);
                }
            }
        }

        // When pruning nodes, we need to remove any nullptr (pruned) children from the vector to keep counts accurate on next loop.
        if (_is_pruning_hwm_nodes) {
            std::vector<Node<T>*>& children = _level_map[child_level];
            children.erase(
                std::remove(children.begin(), children.end(), nullptr),
                children.end()
            );
            children.shrink_to_fit();
        }

        // When pruning parent levels, we need to sweep them out.
        // Nodes default-own children.  We try to avoid this above, but call own_children(false) for safety.
        if (_is_pruning_parent_levels) {
            for (Node<T>* parent : _level_map[parent_level]) {
                parent->own_children(false);
                delete parent;
            }
            _level_map[parent_level].clear();
            _level_map[parent_level].shrink_to_fit();
        }

        // Coverage is tallied above.  If pruning, logic is different because ancestors were purged already, which means
        // their descendents were purged, but still count toward coverage.  Luckily, by shrinking the vector above, we
        // can rely on .size() to tell us how many are *not* covered, which means covered = total - .size().
        T total = step * 2;
        T covered = covered_or_pruned;
        if (_is_pruning_hwm_nodes) {
            covered = total - _level_map[child_level].size();
        }
        _coverage_map[child_level] = BinaryTreeCoverage<T>(covered, total);
    }

    // Helper to determine if the level requested is going to fit within the bit-size of T.
    void assert_level_will_fit(size_t level) const {
        if constexpr(std::integral<T>) {
            size_t bits = std::numeric_limits<T>::digits;
            T max_iv_allowed = CollatzConstants::get_max_initial_value_by_bit(bits);
            size_t max_level_allowed = max_full_level_of_n(max_iv_allowed);
            mpz_class max_iv_needed = max_node_n(level);
            if (max_iv_needed > max_iv_allowed) {
                std::string msg = "Cannot build a BinaryTree with ";
                msg += std::to_string(level) + " levels and type '" + typeid(T).name() + "' with ";
                msg += std::to_string(bits) + " bits. A Collatz sequence will overflow.";
                msg += " Max level for this type is " + std::to_string(max_level_allowed) + ".";
                throw std::out_of_range(msg);
            }
        }
    }

    // Calculate level.  Formula: floor(log2(N+1))
    static size_t level(T value) {
        size_t level = 0;
        if constexpr(std::integral<T>) {
            // Integer truncation will cover us with a static cast.
            level = static_cast<size_t>(std::log2(value + 1));
        } else if constexpr(std::same_as<T, mpz_class>) {
            // GMP doesn't have a logarithm function, but we can exploit sizeinbase() - 1 for the same.
            // Adding 1 is a waste of alloc here, so use a scratch variable.
            mpz_class junk = 0;
            // static thread_local mpz_class junk = 0;
            mpz_add_ui(junk.get_mpz_t(), value.get_mpz_t(), 1);
            level = mpz_sizeinbase(junk.get_mpz_t(), 2) - 1;
        }
        return level;
    }

    // Calculate max node N value at a level.  Formula: 2^level - 2.
    static mpz_class max_node_n(size_t level) {
        mpz_class max_n;
        mpz_pow_ui(max_n.get_mpz_t(), CollatzConstants::MPZ_TWO.get_mpz_t(), level);
        max_n -= 2;
        return max_n;
    }

    // Max FULL level of a BinaryTree we can build with node N as the highest initial value before failure/overflow.
    // Formula: level(max_iv + 1) - 1.
    //    i.e.: level of (failure point) - 1 == safest level we can fully build.
    static size_t max_full_level_of_n(mpz_class max_iv) {
        return BinaryTree<mpz_class>::level(max_iv + 1) - 1;
    }

    // Generate any Node based on its level and position.  It will not be part of any tree.
    // Throws errors when you ask for invalid positions in a node.
    static Node<T>* generate_node_at(size_t level, T position, bool with_metadata = true) {
        // Calculate the maximum position and enforce the rules.  We will need the first node's value too.
        T max_position = 0;
        T first_node_value = 0;
        if constexpr(std::integral<T>) {
            max_position = 1ULL << level;
            first_node_value = (1ULL << level) - 1;
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
            value = 1ULL << (level - 1);
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
                value = (1ULL << n) - 3;
                magnitude = 1ULL << (level - n);
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
        Node<T>* node = new Node<T>(node_value, with_metadata);
        return node;
    }
};
