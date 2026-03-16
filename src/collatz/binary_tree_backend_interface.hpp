#pragma once

#include <climits>
#include <cstddef>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <string>
#include "concepts.hpp"
#include "node.hpp"
#include "binary_tree_math.hpp"
#include "binary_tree_coverage.hpp"



template<AnySupportedIntegral T>
struct IBinaryTreeBackend {
    virtual ~IBinaryTreeBackend() = default;

    virtual void init(size_t levels) = 0;
    virtual void add_level() = 0;
    virtual void reset() = 0;



    //
    // Getters and Accessors
    //
    virtual T node_count() const = 0;
    virtual size_t get_level_count() const = 0;
    virtual Node<T>* get_root_node() const = 0;
    virtual const std::unordered_map<size_t, std::vector<Node<T>*>>& get_level_map() const = 0;
    virtual const std::unordered_map<size_t, BinaryTreeCoverage<T>>& get_coverage_map() const = 0;
    virtual const std::vector<Node<T>*> get_ancestors() const = 0;
    virtual size_t deep_size() const = 0;
    //
    // Property exposure.
    virtual bool tracking_metadata() const { return false; }
    virtual bool is_verifying_non_hwm_nodes() const = 0;
    virtual void disable_non_hwm_node_verification() = 0;
    virtual void enable_non_hwm_node_verification() = 0;
    // (Materialized Only)
    virtual bool is_pruning_hwm_nodes() const = 0;
    virtual bool is_pruning_parent_levels() const = 0;
    // (Implicit Only)
    virtual const std::vector<Interval<T>>& get_uncovered_intervals() const = 0;



    //
    // Assert Level Fits
    // Helper to determine if the level requested is going to fit within the bit-size of T.  Only applies to native integrals.
    //
    void assert_level_will_fit(size_t level) const {
        if (! BinaryTreeMath<T>::st_level_will_fit(level)) {
            size_t bits = sizeof(T) * 8;
            size_t max_level = bits - 1 - (std::is_signed_v<T> ? 1 : 0);
            std::string msg = "Cannot build a BinaryTree with ";
            msg += to_string_any(level) + " levels and type '" + typeid(T).name() + "' with ";
            msg += to_string_any(bits) + " bits. A Collatz sequence will overflow.";
            msg += " Max level for this type is " + to_string_any(max_level) + ".";
            throw std::out_of_range(msg);
        }
    }



    //
    // Assert Verification
    // If verification is disabled (default) and the tree level exceeds the max known, fail.  It might seen annoying, but we cannot
    // build an API that allows people to build trees beyond empirically tested boundaries unless they are willing to do verify
    // new nodes themselves (which they can with disable_non_hwm_node_verification).
    //
    void assert_level_verification(size_t level, bool is_verifying_non_hwm_nodes) {
        if (is_verifying_non_hwm_nodes == false && level == CollatzConstants::LARGEST_EPIRICALLY_TESTED_LEVEL + 1) {
            std::string msg= "Tree has reached max level of previously verified space: "
            + to_string_any(level)
            + ".  You have disabled runtime verification, which means this and future levels are NOT fully verified!"
            + "  Refusing to build level.";
            throw std::runtime_error(msg);
        }
    }
};
