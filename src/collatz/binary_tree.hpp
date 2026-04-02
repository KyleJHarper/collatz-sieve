#pragma once

#include "binary_tree_materialized.hpp"
#include "binary_tree_implicit.hpp"





//
// The BinaryTree facade, enabling users to have a standard interface regardless of the tree type they want.
//
template<AnySupportedIntegral T>
class BinaryTree {
    private:
    std::unique_ptr<IBinaryTreeBackend<T>> _impl;
    BinaryTreeType _tree_type = BinaryTreeType::MATERIALIZED;

    public:
    static constexpr BinaryTreeOptions DEFAULT_OPTS{};
    BinaryTree() = default;
    BinaryTree(size_t levels, const BinaryTreeOptions& opts = DEFAULT_OPTS) {
        init(levels, opts);
    }


    //
    // Method delegates for facade.
    //
    void init(size_t levels, const BinaryTreeOptions& opts = DEFAULT_OPTS) {
        reset();

        // Create either a materialized or implicit tree.
        if (opts.tree_type == BinaryTreeType::MATERIALIZED) {
            _tree_type = BinaryTreeType::MATERIALIZED;
            _impl = std::make_unique<BinaryTreeMaterialized<T>>(opts);
        } else {
            _tree_type = BinaryTreeType::IMPLICIT;
            _impl = std::make_unique<BinaryTreeImplicit<T>>(opts);
        }
        _impl->init(levels);
    }
    void reset() { _impl.reset(); }
    void add_level() { _impl->add_level(); }
    void assert_level_will_fit(size_t level) const { _impl->assert_level_will_fit(level); }
    void assert_materialized(std::string caller) const {
        if (_tree_type != BinaryTreeType::MATERIALIZED) {
            throw std::logic_error("The method you're trying to call (" + caller + ") is only available for materialized trees.");
        }
    }
    void assert_implicit(std::string caller) const {
        if (_tree_type != BinaryTreeType::IMPLICIT) {
            throw std::logic_error("The method you're trying to call (" + caller + ") is only available for implicit trees.");
        }
    }


    //
    // Getters and Accessors
    //
    T node_count() const { return _impl->node_count(); }
    size_t get_level_count() const { return _impl->get_level_count(); }
    Node<T>* get_root_node() const { return _impl->get_root_node(); }
    const std::unordered_map<size_t, BinaryTreeCoverage<T>>& get_coverage_map() const { return _impl->get_coverage_map(); }
    const std::vector<Node<T>*> get_ancestors() const { return _impl->get_ancestors(); }
    size_t deep_size() const { return _impl->deep_size(); }
    bool is_verifying_non_hwm_nodes() const { return _impl->is_verifying_non_hwm_nodes(); }
    void disable_non_hwm_node_verification() { _impl->disable_non_hwm_node_verification(); }
    void enable_non_hwm_node_verification() { _impl->enable_non_hwm_node_verification(); }
    //
    // Materialized-Specific
    const std::unordered_map<size_t, std::vector<Node<T>*>>& get_level_map() const {
        assert_materialized("get_level_map");
        return _impl->get_level_map();
    }
    bool is_pruning_hwm_nodes() const {
        assert_materialized("is_pruning_hwm_nodes");
        return _impl->is_pruning_hwm_nodes();
    }
    bool is_pruning_parent_levels() const {
        assert_materialized("is_pruning_parent_levels");
        return _impl->is_pruning_parent_levels();
    }
    //
    // Implicit-Specific
    const NodeBitmap<T>& get_uncovered_positions() const {
        assert_implicit("get_uncovered_positions");
        return _impl->get_uncovered_positions();
    }

};
