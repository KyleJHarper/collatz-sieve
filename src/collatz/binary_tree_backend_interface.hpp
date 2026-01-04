#pragma once

#include <cstddef>
#include <unordered_map>
#include <vector>
#include <string>
#include "collatz.hpp"
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
    // (Materialized Only)
    virtual bool is_pruning_hwm_nodes() const = 0;
    virtual bool is_pruning_parent_levels() const = 0;
    // (Implicit Only)
    virtual const std::unordered_map<size_t, std::vector<Interval<T>>>& get_covered_intervals() const = 0;



    //
    // Assert Level Fits
    // Helper to determine if the level requested is going to fit within the bit-size of T.  Only applies to native integrals.
    //
    void assert_level_will_fit(size_t level) const {
        if (! BinaryTreeMath<T>::st_level_will_fit(level)) {
            size_t bits = sizeof(T) * 8;
            T max_iv_allowed = CollatzConstants::get_max_initial_value_by_bit<T>(bits);
            mpz_class max_iv_allowed_mpz;
            if constexpr(NativeIntegral<T>) {
                max_iv_allowed_mpz = max_iv_allowed;
            } else if constexpr(ExtendedIntegral<T>) {
                uint128_to_mpz(max_iv_allowed, max_iv_allowed_mpz);
            }
            size_t max_level_allowed = BinaryTreeMath<T>::st_max_full_level_at_node(max_iv_allowed);
            std::string msg = "Cannot build a BinaryTree with ";
            msg += to_string_any(level) + " levels and type '" + typeid(T).name() + "' with ";
            msg += to_string_any(bits) + " bits. A Collatz sequence will overflow.";
            msg += " Max level for this type is " + to_string_any(max_level_allowed) + ".";
            throw std::out_of_range(msg);
        }
    }

};
