#pragma once

#include <cmath>
#include <gmp.h>
#include <gmpxx.h>
#include <stdexcept>
#include <unordered_map>
#include <omp.h>
#include "node.hpp"
#include "binary_tree_coverage.hpp"
#include "slab.hpp"


//
// A perfect binary tree mapped to powers of two.  This creates a uniform distribution of nodes in
// the N+/Z space (positive integers), which Collatz is concerned.
//
template<IntegralOrMPZClass T>
class BinaryTree {
    private:
    static constexpr size_t MAX_THREADS = 256;
    static constexpr size_t ALLOCATOR_SLAB_SIZE = 1024;
    Node<T> *_root_node = nullptr;
    size_t _max_level = 0;
    std::unordered_map<size_t, std::vector<Node<T>*>> _level_map;
    std::unordered_map<size_t, BinaryTreeCoverage> _coverage_map;
    std::vector<SlabAllocator<Node<T>>> _allocators;

    // Get the allocator for our thread.
    SlabAllocator<Node<T>>& get_thread_allocator() {
        int tid = omp_get_thread_num();
        // std::cout << "Trying to use tid " << tid << " and allocator has " << _allocators[tid].allocated_count() << " items." << std::endl;
        return _allocators[tid];
    }

    public:
    // Constructor
    BinaryTree(size_t levels) {
        // Build the slab allocators.
        _allocators.reserve(MAX_THREADS);
        if(omp_get_max_threads() > int(MAX_THREADS)) {
            throw std::runtime_error("Number of OMP threads in BinaryTree exceeds MAX_THREADS value.");
        }
        for (size_t i = 0 ; i < MAX_THREADS ; i++) {
            _allocators.emplace_back(ALLOCATOR_SLAB_SIZE);
        }
        _root_node = _allocators[0].allocate();
        new (_root_node) Node<T>(0, nullptr);
        _level_map[0].resize(1);
        _level_map[0][0] = _root_node;
        _coverage_map[0].set_covered(0);
        for (size_t level = 1; level <= levels; ++level) {
            this->add_level();
        }
    }
    // Destructor
    // We need to destroy the root node, since we're the ones who created it in our constructor.
    // However, Node objects replicated destruction to children, so we don't need to walk the tree
    // ourself.
    ~BinaryTree() {
        // Don't use `delete`.  These are placement-new constructed, not new constructed.
        // The allocator will handle destruction when clear is called.
        // Reset all the allocators so the Node destructors are called.
        // for (size_t i = 0; i < MAX_THREADS; i++) {
        //     _allocators[i].reset();
        // }
    }


    // Accessors and properties.
    const size_t& get_max_level() const {
        return _max_level;
    }
    Node<T>* get_root_node() const {
        return _root_node;
    }
    const std::unordered_map<size_t, std::vector<Node<T>*>>& get_level_map() const {
        return _level_map;
    }
    const T node_count() const {
        // It should be: 2^(max_levels + 1) - 1 (if we count node 0)
        if constexpr(std::integral<T>) {
            return (std::pow(2, _max_level + 1) - 2);
        } else if constexpr(std::same_as<T, mpz_class>) {
            T result = 0;
            mpz_ui_pow_ui(result.get_mpz_t(), 2, _max_level + 1);
            result = result - 2;
            return result;
        }
        throw std::runtime_error("Unable to determine type for calculating node_count().");
    }
    const T node_count_with_root() const {
        return node_count() + 1;
    }
    size_t deep_size() const {
        size_t total = sizeof(*this);
        // Account for each level and its vector of node pointers
        for (const auto& [level, nodes_vec] : _level_map) {
            total += sizeof(level);
            total += sizeof(nodes_vec);
            total += nodes_vec.capacity() * sizeof(Node<T>*);
            // For each Node*, include its deep size
            for (const Node<T>* node_ptr : nodes_vec) {
                if (node_ptr) {
                    total += node_ptr->deep_size();
                }
            }
        }
        // Account for coverage map
        for (const auto& [level, coverage] : _coverage_map) {
            total += sizeof(level);
            total += sizeof(coverage);
        }
        return total;
    }
    const std::unordered_map<size_t, BinaryTreeCoverage> get_coverage_map() const {
        return _coverage_map;
    }

    // Add a level to the tree.  We simply take the parent nodes and add two children with a steady
    // step value.  We also calculate the coverage for this level.
    void add_level() {
        // Get the parent and child level IDs.
        size_t parent_level = _max_level;
        size_t child_level = _max_level + 1;
        _max_level++;
        // Each level will double the size of the tree, so we can't rely on size_t if we're going to
        // support GMP-size values.  We need to respect T.
        size_t step = 1ULL << parent_level;
        // Loop through the parents to build the children.
        auto& parents = _level_map[parent_level];
        size_t parent_count = parents.size();
        size_t child_count = parent_count * 2;
        _level_map[child_level].resize(child_count);
        #pragma omp parallel for schedule(dynamic, 1) default(none) shared(parents, _level_map, step, child_level, parent_count)
        for(size_t parent_idx = 0; parent_idx < parent_count; parent_idx++) {
            // Find our allocator.
            auto& allocator = get_thread_allocator();
            // Get the child values.
            Node<T>* parent = parents[parent_idx];
            T child_value_1 = parent->get_value() + step;
            T child_value_2 = child_value_1 + step;
            // Create the children.  Add them to the map.
            Node<T>* child_1 = allocator.allocate();
            Node<T>* child_2 = allocator.allocate();
            new (child_1) Node<T>(child_value_1, parent); // placement-new construct
            new (child_2) Node<T>(child_value_2, parent);
            _level_map[child_level][2 * parent_idx] = child_1;
            _level_map[child_level][2 * parent_idx + 1] = child_2;
            // Add children to the parent.
            parent->assign_child(child_1);
            parent->assign_child(child_2);
        }
        // Establish the coverage.  Covered is always 0, but total is simply step * 2.
        _coverage_map[child_level].set_covered(0);
        _coverage_map[child_level].set_total(step * 2);
        for(const Node<T>* child_node : _level_map[child_level]) {
            if(child_node->is_below_high_water_mark() || child_node->has_high_water_mark_ancestor()) {
                _coverage_map[child_level].add_covered(1);
            }
        }
    }

    // Generate any Node based on its level and position.  It will not be part of any tree.
    // Throws errors when you ask for invalid positions in a node.
    static Node<T>* generate_node_at(size_t level, T position) {
        // Calculate the maximum position and enforce the rules.  We will need the first node's value too.
        T max_position = 0;
        T first_node_value = 0;
        if constexpr(std::integral<T>) {
            max_position = std::pow(2, level);
            first_node_value = std::pow(2, level) - 1;
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
            value = std::pow(2, level - 1);
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
                value = std::pow(2, n) - 3;
                magnitude = std::pow(2, level - n);
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
        Node<T>* node = new Node<T>(node_value);
        return node;
    }
};
