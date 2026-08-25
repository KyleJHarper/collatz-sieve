#pragma once


#include <stdexcept>
#include <unordered_map>
#include "concepts.hpp"
#include "for_each.hpp"
#include "string.hpp"
#include "equality_helper.hpp"
#include "node.hpp"
#include "binary_tree_math.hpp"
#include "binary_tree_coverage.hpp"
#include "binary_tree_options.hpp"
#include "binary_tree_types.hpp"
#include "stream_helper.hpp"
#include <omp.h>
#include <tbb/parallel_sort.h>
#include "node_bitmap.hpp"




/**
* @class BinaryTreeMaterializedImpl
* @brief An implementation of the `BinaryTree` which creates and persists `Node` objects in memory.
*
* This is the materialized version, which creates real Node objects and persists them.  When pruning is enabled, nodes which hit
* the Abort at Stopping Time (AST) are removed, leaving only nodes which still need to be tested.
*
* @note This was a first-principles implementation when starting out.  It's great for in-memory inspection of true `Node` objects,
* or when you want to walk an actual tree, but RAM explodes quickly.  If you need to work with larger trees, consider using the
* `BinaryTreeImplicitImpl` instead.
*
* @tparam T Any supported integral (see concepts.hpp).
*/
template<AnySupportedIntegral T>
class BinaryTreeMaterializedImpl {
    private:
    /// @brief The root of a tree.  Navigable in this tree type.
    Node<T>* _root_node = nullptr;
    /// @brief Number of levels in the tree.
    level_t _level_count = 0;
    /// @brief The main storage for `Node` objects, placed in a map keyed by level.
    std::unordered_map<level_t, std::vector<Node<T>*>> _level_map;
    /// @brief Bitmap containing a list of all values still not covered.
    NodeBitmap<T> _uncovered_values;
    /// @brief Map of levels and their associated coverage.
    std::unordered_map<level_t, BinaryTreeCoverage<T>> _coverage_map;
    /// @brief Collection of ancestors, which are Abort at Stopping Time nodes that originally truncated a subtree.
    std::vector<Node<T>*> _ancestors;
    /// @brief Flag to determine if this tree is initialized, meaning it's had at least one level added.
    bool _is_initialized = false;
    /// @brief Flag to determine if the tree should prune AST nodes, carving out subtress, turning this into a DAG.
    bool _is_pruning_ast_nodes = BinaryTreeOptions{}.prune_ast_nodes;
    /// @brief Flag to determine if the tree should prune parent levels.  AKA, only keep the last level (leaf nodes).
    bool _is_pruning_parent_levels = BinaryTreeOptions{}.prune_parent_levels;
    /// @brief Flag to determine if AST ancestors should be added to our vector or simply discarded.
    bool _is_preserving_ancestors = BinaryTreeOptions{}.preserve_ancestors;
    /// @brief Flag to determine if verification of non-AST nodes should happen.  See `BinaryTree` or CHANGELOG 3.4.0 for details.
    bool _is_verifying_non_ast_nodes = BinaryTreeOptions{}.verify_non_ast_nodes;
    /// @brief Track the type of this tree for external comparison later.
    const TreeTypeEnum _tree_type = TreeTypeEnum::MATERIALIZED;



    public:
    /// @name Lifecycle Management
    /// @{

    /**
    * @brief Default constructor.
    */
    BinaryTreeMaterializedImpl() = default;


    /// @brief Disallow copying.
    BinaryTreeMaterializedImpl(const BinaryTreeMaterializedImpl&) = delete;
    /// @brief Disallow copying.
    BinaryTreeMaterializedImpl& operator=(const BinaryTreeMaterializedImpl&) = delete;



    /// @brief Moving is okay.
    BinaryTreeMaterializedImpl(BinaryTreeMaterializedImpl&&) noexcept = default;
    /// @brief Moving is okay.
    BinaryTreeMaterializedImpl& operator=(BinaryTreeMaterializedImpl&&) noexcept = default;



    /**
    * @brief Destructor will remove any ancestors and clear the level map.
    */
    ~BinaryTreeMaterializedImpl() {
        // Clean up ancestors.  These are new-placement, so they aren't linked to the same memory as _level_map.
        for (Node<T>* node : _ancestors) {
            delete node;
        }
        _ancestors.clear();

        // Now clear out the level_map.  If pruning, handle it carefully.
        if (_is_pruning_ast_nodes) {
            // Scan all levels manually and call delete explicitly.
            for (level_t level = 0; level <= _level_count; level++) {
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



    /**
    * @brief Reset members to make this act like a new object.  Mostly for `init()`.
    */
    void reset() {
        _is_initialized = false;
        _coverage_map.clear();
        _level_map.clear();
        _uncovered_values.clear();
        _level_count = 0;
        for (Node<T>* ancestor : _ancestors) {
            delete ancestor;
        }
        _ancestors.clear();
        if (_root_node != nullptr) {
            _root_node = nullptr;
        }
        _is_verifying_non_ast_nodes = BinaryTreeOptions{}.verify_non_ast_nodes;
        _is_pruning_ast_nodes = BinaryTreeOptions{}.prune_ast_nodes;
        _is_pruning_parent_levels = BinaryTreeOptions{}.prune_parent_levels;
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
        _is_verifying_non_ast_nodes = opts.verify_non_ast_nodes;
        _is_pruning_ast_nodes = opts.prune_ast_nodes;
        _is_pruning_parent_levels = opts.prune_parent_levels;
        _is_preserving_ancestors = opts.preserve_ancestors;
        while (_level_count < levels) {
            this->add_level();
        }
    }

    /// @}



    /// @name Accessors Common to All Trees
    /// @{

    // Common properties.
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
    void set_is_preserving_ancestors(const bool value) { _is_preserving_ancestors = value; }

    /// @brief Get a flag whether this tree is verifying non-AST nodes.
    bool is_verifying_non_ast_nodes() const { return _is_verifying_non_ast_nodes; }
    /// @brief Explicitly disable non-AST node verification.
    void disable_non_ast_node_verification() { _is_verifying_non_ast_nodes = false; }
    /// @brief Explicitly enable non-AST node verification.
    void enable_non_ast_node_verification() { _is_verifying_non_ast_nodes = true; }

    /// @brief Get a flag whether this tree is initialized.
    bool is_initialized() const { return _is_initialized; }
    /// @brief Change the flag for whether this tree is initialized.
    /// @warning This is intended for serialization/deserilization only.
    /// @param value The true or false value to set.
    void set_is_initialized(const bool value) { _is_initialized = value; }

    /// @brief Get a reference to the uncovered values, which are the node values not covered by an AST ancestor.
    const NodeBitmap<T>& get_uncovered_values() const { return _uncovered_values; }
    /// @brief Get a read-write reference to the uncovered values map.
    /// @warning This is intended for serialization/deserilization only.
    NodeBitmap<T>& get_uncovered_values_rw() { return _uncovered_values; }
    /// @brief Clears the value map.  Usually for debugging.
    void clear_uncovered_values() { _uncovered_values.clear(); }

    /// @}



    /// @name Materialized-Only Accessors
    /// @{

    /// @brief Responds true to `is_materialized()`, mostly for the `BinaryTree` facade.
    bool is_materialized() const { return true; }

    /// @brief Get a reference to the level map, which contains all of the `Node` objects.
    /// @note Nodes are left-to-right by position, not node value.
    const std::unordered_map<level_t, std::vector<Node<T>*>>& get_level_map() const { return _level_map; }

    /// @brief Get a flag whether this tree is pruning AST nodes.
    bool is_pruning_ast_nodes() const { return _is_pruning_ast_nodes; }

    /// @brief Get a flag whether this tree is pruning parent levels, leaving only the last one (leaf nodes).
    bool is_pruning_parent_levels() const { return _is_pruning_parent_levels; }

    /// @brief Returns the REAL node count in the _level_map.
    T real_node_count() const {
        T total = 0;
        for (level_t level = 1; level <= _level_count; level++) {
            total += _level_map.at(level).size();
        }
        return total;
    }

    /// @}



    /**
    * @brief Deeply scan the object and report its size.
    */
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

        // Add uncovered values bitmap.
        total += _uncovered_values.deep_size();

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



    /**
    * @brief Compare two materialized trees' specific internals and return true if identical.
    *
    * This method only tests materialized tree specifics.  Most equality happens via the `BinaryTree` facade since they share
    * common members between implementations (e.g.: level count).
    *
    * This function checks:
    *   1. The is_pruning_ast_nodes flag.
    *   2. The is_pruning_parent_levels flag.
    *   3. Level map (`Node` objects, which invoke their own `Node::st_equal()`).
    *
    * @param first The first tree to compare.
    * @param second The second tree to compare.
    * @param err Pointer to a string where inequality or error messages are stored.
    * @return True if equal, false otherwise.
    */
    static bool st_equal(const BinaryTreeMaterializedImpl<T>& first, const BinaryTreeMaterializedImpl<T>& second, std::string* err = nullptr) {
        EqualityHelper eq(err);
        eq.set_category("BinaryTreeMaterializedImpl");

        // Flags
        if (! eq.equal(first.is_pruning_ast_nodes(), second.is_pruning_ast_nodes())) {
            return eq.fail("Is pruning ast nodes mismatch");
        }
        if (! eq.equal(first.is_pruning_parent_levels(), second.is_pruning_parent_levels())) {
            return eq.fail("Is pruning parent levels mismatch");
        }

        // Level Map
        if (! eq.equal(first.get_level_map().size(), second.get_level_map().size())) {
            return eq.fail("Level map count mismatch");
        }
        for (const auto& [level, f_nodes] : first.get_level_map()) {
            if (! second.get_level_map().contains(level)) {
                return eq.fail("First level map has level " + to_string_any(level) + ", but the second level map doesn't.");
            }
            const std::vector<Node<T>*>& s_nodes = second.get_level_map().at(level);
            if (! eq.equal(f_nodes.size(), s_nodes.size())) {
                return eq.fail("Node counts on level " + to_string_any(level) + " mismatch");
            }
            for (size_t i = 0; i < f_nodes.size(); i++) {
                if(! Node<T>::st_equal(f_nodes.at(i), s_nodes.at(i), err)) {
                    return false;
                }
            }
        }

        // All good.
        return true;
    }



    /**
    * @brief Compare another materialized tree to this one.
    *
    * This is a member helper which simply forwards to `BinaryTreeMaterializedImpl::st_equal()`.
    *
    * @param second The second tree to compare against this.
    * @param err Pointer to a string where inequality or error messages are stored.
    * @return True if equal, false otherwise.
    */
    bool equal(const BinaryTreeMaterializedImpl<T>& second, std::string* err = nullptr) const {
        return BinaryTreeMaterializedImpl<T>::st_equal(*this, second, err);
    }



    /**
    * @brief Serialize the materialized tree specifics of this object for export.
    *
    * Serialization happens in this order:
    *   1. The is_pruning_ast_nodes flag.
    *   2. The is_pruning_parent_levels flag.
    *   3. The level map, which emits level numbers and node counts, and then uses `Node::serialize()` on each node.
    *
    * @note The `BinaryTree` facade handles serialization of common items between implementations.
    * @note This method does not throw.
    * @warning Serialization of pruned trees is not supported.
    * @param out The stream to write data to.
    * @param err Pointer to a string where errors, if any, are written.
    * @return A boolean indicating success or failure.  Do not discard.
    */
    [[nodiscard]] bool serialize(std::ostream& out, std::string* err = nullptr) const {
        // !!! BIG FAT ANNOYING ERROR !!!
        if (_is_pruning_ast_nodes || _is_pruning_parent_levels) {
            std::string msg = "You've activated my trap card!";
                msg+= "  Serializing and deserializing a materialized tree is already a bad idea.";
                msg+= "  Doing it with any form of node pruning enabled is NOT supported NOR a good idea.";
                msg+= "  Oh, and rebuilding a materialized tree is almost always faster than trying to import one.";
                msg+= "  Be well.";
            throw std::logic_error(msg);
        }

        StreamHelper sh(nullptr, &out, err);
        sh.set_category("BinaryTreeMaterializedImpl");

        // Unique properties.
        if (! sh.serialize_bool(_is_pruning_ast_nodes)) {
            return sh.fail("_is_pruning_ast_nodes==" + std::to_string(_is_pruning_ast_nodes));
        }
        if (! sh.serialize_bool(_is_pruning_parent_levels)) {
            return sh.fail("_is_pruning_parent_levels==" + std::to_string(_is_pruning_parent_levels));
        }

        // Level Map
        // Write how many levels we need to loop through later.
        //
        // Note: do NOT serialize level 1, because it's only the root node, which is a common item.  It is linked in deserialize().
        uint64_t u64_level_map_count = get_level_map().size() - 1;
        if (! sh.serialize_integral(u64_level_map_count)) {
            return sh.fail("level_map_count==" + to_string_any(u64_level_map_count));
        }
        // Get the level numbers in order for guaranteed write order.
        std::vector<level_t> level_map_levels;
        level_map_levels.reserve(_level_map.size());
        for (const auto& [level, _] : _level_map) {
            if (level == 1) { continue; }
            level_map_levels.push_back(level);
        }
        std::sort(level_map_levels.begin(), level_map_levels.end());
        // Loop through each level found.
        for (level_t level : level_map_levels) {
            const std::vector<Node<T>*>& nodes = _level_map.at(level);
            // Write the level number and then the number of nodes on it.
            if (! sh.serialize_integral(level)) {
                return sh.fail("level map's level==" + to_string_any(level));
            }
            uint64_t u64_node_size = static_cast<uint64_t>(nodes.size());
            if (! sh.serialize_integral(u64_node_size)) {
                return sh.fail("level map on level==" + to_string_any(level) + " nodes.size()==" + to_string_any(nodes.size()));
            }
            // Write each node out.
            for (const Node<T>* node : nodes) {
                if (! node->serialize(out, err)) {
                    return sh.fail("level map node with value==" + to_string_any(node->get_value()));
                }
            }
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
        sh.set_category("BinaryTreeMaterializedImpl");

        // Unique properties
        bool b_tmp;
        if (! sh.deserialize_bool(b_tmp)) {
            return sh.fail("couldn't read _is_pruning_ast_nodes");
        }
        _is_pruning_ast_nodes = b_tmp;
        if (! sh.deserialize_bool(b_tmp)) {
            return sh.fail("couldn't read _is_pruning_parent_levels");
        }
        _is_pruning_parent_levels = b_tmp;

        // Level map
        // Start by assigning the root node to level 1, because it's not exported in serialize().
        _level_map[1].resize(1);
        _level_map[1][0] = _root_node;
        // Now get the remaining levels, if any.
        uint64_t u64_level_map_count = 0;
        T parent_v;
        T ast_ancestor_v;
        uint8_t child_count;
        if (! sh.deserialize_integral(u64_level_map_count)) {
            return sh.fail("couldn't read level_map_count");
        }
        for (uint64_t i = 0; i < u64_level_map_count; i++) {
            uint64_t parent_index = 0;
            level_t level = 0;
            if (! sh.deserialize_integral(level)) {
                return sh.fail("couldn't read level for index i==" + to_string_any(i));
            }
            uint64_t u64_node_size = 0;
            if (! sh.deserialize_integral(u64_node_size)) {
                return sh.fail("couldn't read node size for level==" + to_string_any(level));
            }
            size_t node_size = static_cast<size_t>(u64_node_size);
            _level_map[level].reserve(node_size);
            for (uint64_t j = 0; j < u64_node_size; j++) {
                // Make a new heap-allocated node to deserialize into.
                Node<T>* new_node = new Node<T>();
                if (! new_node->deserialize(in, parent_v, ast_ancestor_v, child_count, err)) {
                    return sh.fail("couldn't get node on level==" + to_string_any(level) + " and position j==" + to_string_any(j));
                }
                // Add it to the map.
                _level_map[level].push_back(new_node);
                // Find the parent by-value.  Must be in the previous level.  Use while() to avoid GMP allocs.
                // These were added left-to-right, so we can scan with an index efficiently.
                Node<T>* parent = nullptr;
                while (parent_index < _level_map[level - 1].size()) {
                    if (_level_map[level - 1][parent_index]->get_value() == parent_v) {
                        parent = _level_map[level - 1][parent_index];
                        break;
                    }
                    // Didnt' find it, bump the index.
                    parent_index++;
                }
                if (parent == nullptr) {
                    return sh.fail("couldn't find parent on level-1==" + to_string_any(level - 1) + " with parent value==" + to_string_any(parent_v));
                }
                // Link up parents and children.
                new_node->assign_parent(parent);
                parent->assign_child(new_node);
                // Follow the same ancestor tracking logic from Node: inherit parent's, and if still null, see if parent is one.
                new_node->assign_ast_ancestor(parent->get_ast_ancestor());
                if (new_node->get_ast_ancestor() == nullptr && parent->is_below_ast()) {
                    new_node->assign_ast_ancestor(parent);
                }
            }
        }

        // All good
        return true;
    }



    /**
    * @brief Add a level to the tree.
    *
    * Materialized tree building happens by taking the nodes from the last level in `_level_map` and treating them as parents.  The
    * children created have known values according to Harper's algorithm.  The children are checked for Abort at Stopping Time, and
    * stored in the next level of the map.
    *
    * When AST pruning is anbled, all grandparents who meet AST are trimmed, which prevents entire subtrees of nodes from taking up
    * memory.
    *
    * When parent level pruning is enabled, all parents are trimmed, which keeps only the last level of nodes in memory.
    */
    void add_level() {
        // Ensure initialization is set.
        _is_initialized = true;

        // Clear the value bitmap since it would otherwise be stale/incorrect.
        _uncovered_values.clear();

        // When we have no levels, this simply crafts a 0- or 1-based root node and manually sets level map and coverage.
        if (_level_count == 0) {
            _level_count = 1;
            _root_node = new Node<T>(BinaryTreeMath<T>::get_root_value());
            _level_map[_level_count].resize(1);
            _level_map[_level_count][0] = _root_node;
            _coverage_map[_level_count].set_covered(0);
            _coverage_map[_level_count].set_total(1);
            return;
        }

        // Get the parent and child levels.  Then confirm the new one will fit.  Then bump count.
        level_t parent_level = _level_count;
        level_t child_level = _level_count + 1;
        _level_count++;

        // Build the scaling factor value.  Each level doubles the tree, so we need to respect T with GMP-size values.
        T scaling_factor = BinaryTreeMath<T>::st_scaling_factor(child_level);

        // Calculate parent and child counts for looping and indexing.
        // Note: size_t is safe, because no one can fit 2^64 nodes in RAM.
        auto& parents = _level_map[parent_level];
        size_t parent_count = parents.size();
        size_t child_count = parent_count * 2;

        // Resize the vector to contain all children.  Otherwise assigning to indexes ([]) will fail.
        _level_map[child_level].resize(child_count);

        // Keep a record of covered or pruned values for coverage math after the loop.  Use guaranteed-width integral.
        uint64_t covered_or_pruned = 0;

        // Create an external tracker for ancestors for OMP local threads to utilize by-ref.
        std::vector<std::vector<Node<T>*>> omp_local_ancestors_group(omp_get_max_threads());

        // Begin the critical section, but don't loop yet.
        std::exception_ptr eptr = nullptr;
        #pragma omp parallel reduction(+:covered_or_pruned) default(none) shared(parents, _level_map, scaling_factor, child_level, parent_count, _is_pruning_ast_nodes, _is_preserving_ancestors, omp_local_ancestors_group, _is_verifying_non_ast_nodes, eptr)
        {
            try {
                // Use local child values.
                T child_value_1;
                T child_value_2;

                // Grab this thread's personal ancestors vector to avoid locking.
                auto& my_ancestors = omp_local_ancestors_group[omp_get_thread_num()];

                // Track this thread's local AST.  Since nodes are not sequentially ordered, it can only set this to first node - 1.
                T my_ast_value = BinaryTreeMath<T>::st_first_node_value_of_level(_level_count) - 1;
                if (my_ast_value < 1) {
                    my_ast_value = 1;
                }

                // Now Loop
                #pragma omp for schedule(static)
                for(size_t parent_idx = 0; parent_idx < parent_count; parent_idx++) {
                    // Find the parent.
                    Node<T>* parent = parents[parent_idx];

                    // Compute the child values.  Avoid alloc() with GMP with arithmetic operators.
                    if constexpr(FixedWidthIntegral<T>) {
                        child_value_1 = parent->get_value() + scaling_factor;
                        child_value_2 = child_value_1 + scaling_factor;
                    } else if constexpr(GMPIntegral<T>) {
                        mpz_add(child_value_1.get_mpz_t(), parent->get_value().get_mpz_t(), scaling_factor.get_mpz_t());
                        mpz_add(child_value_2.get_mpz_t(), child_value_1.get_mpz_t(), scaling_factor.get_mpz_t());
                    }

                    // Instaniate the children.
                    Node<T>* child_1 = new Node<T>(child_value_1, parent);
                    Node<T>* child_2 = new Node<T>(child_value_2, parent);

                    // Tally them.  Prune them if necessesary.  Otherwise add them to the map.
                    // -- Child 1
                    bool assign_to_map = true;
                    if (_is_preserving_ancestors && child_1->is_below_ast() && ! child_1->has_ast_ancestor()) {
                        my_ancestors.push_back(new Node<T>(child_1->get_value()));
                    }
                    if (_is_verifying_non_ast_nodes) {
                        if (child_1->is_below_ast() == false && child_1->has_ast_ancestor() == false) {
                            if (Collatz<T>::st_verify(child_value_1, my_ast_value) == false) {
                                throw std::logic_error("Node value " + to_string_any(child_value_1) + " didn't verify.  How?");
                            }
                        }
                    }
                    if (child_1->is_below_ast() || child_1->has_ast_ancestor()) {
                        covered_or_pruned += 1;
                        if (_is_pruning_ast_nodes) {
                            delete child_1;
                            child_1 = nullptr;
                            assign_to_map = false;
                        }
                    }
                    if (assign_to_map) {
                        _level_map[child_level][2 * parent_idx] = child_1;
                        if (! _is_pruning_ast_nodes && ! _is_pruning_parent_levels) {
                            parent->assign_child(child_1);
                        }
                    }
                    // -- Child 2
                    assign_to_map = true;
                    if (_is_preserving_ancestors && child_2->is_below_ast() && ! child_2->has_ast_ancestor()) {
                        my_ancestors.push_back(new Node<T>(child_2->get_value()));
                    }
                    if (_is_verifying_non_ast_nodes) {
                        if (child_2->is_below_ast() == false && child_2->has_ast_ancestor() == false) {
                            if (Collatz<T>::st_verify(child_value_2, my_ast_value) == false) {
                                throw std::logic_error("Node value " + to_string_any(child_value_2) + " didn't verify.  How?");
                            }
                        }
                    }
                    if (child_2->is_below_ast() || child_2->has_ast_ancestor()) {
                        covered_or_pruned += 1;
                        if (_is_pruning_ast_nodes) {
                            delete child_2;
                            child_2 = nullptr;
                            assign_to_map = false;
                        }
                    }
                    if (assign_to_map) {
                        _level_map[child_level][2 * parent_idx + 1] = child_2;
                        if (! _is_pruning_ast_nodes && ! _is_pruning_parent_levels) {
                            parent->assign_child(child_2);
                        }
                    }
                }
            } catch (...) {
                #pragma omp critical
                {
                    if(!eptr) {
                        eptr = std::current_exception();
                    }
                }
            }
        }

        // Rethrow errors.
        if(eptr != nullptr) {
            std::rethrow_exception(eptr);
        }

        // Merge Ancestors.
        if (_is_preserving_ancestors) {
            for (std::vector<Node<T>*>& omp_ancestors : omp_local_ancestors_group) {
                _ancestors.insert(_ancestors.end(), omp_ancestors.begin(), omp_ancestors.end());
            }
            // Always sort them.
            tbb::parallel_sort(_ancestors.begin(), _ancestors.end(), [](const Node<T>* a, const Node<T>* b) {
                if constexpr(FixedWidthIntegral<T>) {
                    return a->get_value() < b->get_value();
                } else if constexpr(GMPIntegral<T>) {
                    return mpz_cmp(a->get_value().get_mpz_t(), b->get_value().get_mpz_t()) < 0;
                }
            });
        }

        // When pruning nodes,remove any nullptr (pruned) children from the vector to keep counts accurate on next loop.
        if (_is_pruning_ast_nodes) {
            std::vector<Node<T>*>& children = _level_map[child_level];
            children.erase(
                std::remove(children.begin(), children.end(), nullptr),
                children.end()
            );
            children.shrink_to_fit();
        }

        // When pruning parent levels, sweep them out.
        // Nodes default-own children.  Call own_children(false) for safety.
        if (_is_pruning_parent_levels) {
            // When it's level 1, clear up _root too so it doesn't double-free later.
            if (parent_level == 1) {
                _root_node = nullptr;
            }
            #pragma omp parallel for
            for (size_t i = 0; i < _level_map[parent_level].size(); i++) {
                Node<T>* parent = _level_map[parent_level][i];
                parent->own_children(false);
                delete parent;
            }
            _level_map[parent_level].clear();
            _level_map[parent_level].shrink_to_fit();
        }

        // Coverage is tallied above.  If pruning, logic is different because ancestors were purged already, which means their
        // descendents were purged, but still count toward coverage.  Luckily, by shrinking the vector above, this method can rely
        // on .size() to report how many are *not* covered, which means covered = total - .size().
        T total = scaling_factor * 2;
        T covered = covered_or_pruned;
        if (_is_pruning_ast_nodes) {
            covered = total - _level_map[child_level].size();
        }
        _coverage_map[child_level] = BinaryTreeCoverage<T>(covered, total);
    }



    /**
    * @brief Builds a value map as a `NodeBitmap` for iteration later.
    * @tparam BUFFER_SIZE Controls memory usage of temporary buffers.  Larger values can aid performance at the cost of more memory
    * and vice-versa.
    * @param policy The type of `ForEachPolicy` to build the value map.  Serial is slower but uses less temporary memory.  Parallel
    * uses 3-5x more memory (or more depending on OMP thread count) for only ~2x speedup.  Default is `ForEachPolicy::SERIAL`.
    */
    template<size_t BUFFER_SIZE = 1 << 24>
    void generate_value_map(ForEachPolicy policy = ForEachPolicy::SERIAL) {
        _uncovered_values.clear();

        if (policy == ForEachPolicy::SERIAL) {
            // Just loop through the level map one-by-one.
            std::vector<T> buffer;
            buffer.reserve(BUFFER_SIZE);
            for (const Node<T>* node : _level_map[_level_count]) {
                // Non-pruned trees have covered nodes in memory.  Check for them.
                if (node->is_below_ast() || node->has_ast_ancestor()) {
                    continue;
                }
                buffer.push_back(node->get_value());
                if (buffer.size() >= BUFFER_SIZE) {
                    _uncovered_values.add_many(buffer.size(), buffer.data());
                    buffer.clear();
                }
            }
            _uncovered_values.add_many(buffer.size(), buffer.data());
        } else {
            struct ValueMapTLS {
                NodeBitmap<T> bitmap;
                std::vector<T> buffer;
            };
            std::vector<ValueMapTLS> tls_vector;
            tls_vector.resize(omp_get_max_threads());
            #pragma omp parallel default(none) shared(_level_map, tls_vector)
            {
                int my_thread_id = omp_get_thread_num();
                ValueMapTLS& my_tls = tls_vector[my_thread_id];
                my_tls.buffer.reserve(BUFFER_SIZE);
                #pragma omp for
                for (size_t i = 0; i < _level_map[_level_count].size(); i++) {
                    my_tls.buffer.push_back(_level_map[_level_count][i]->get_value());
                    if (my_tls.buffer.size() >= BUFFER_SIZE) {
                        my_tls.bitmap.add_many(my_tls.buffer.size(), my_tls.buffer.data());
                        my_tls.buffer.clear();
                    }
                }
                my_tls.bitmap.add_many(my_tls.buffer.size(), my_tls.buffer.data());
            }
            // Now merge bitmaps and remaining buffers.
            for (ValueMapTLS& tls : tls_vector) {
                _uncovered_values |= tls.bitmap;
                tls.bitmap.clear();
                _uncovered_values.add_many(tls.buffer.size(), tls.buffer.data());
            }
        }

        // Optimize
        _uncovered_values.optimize();
    }

};
