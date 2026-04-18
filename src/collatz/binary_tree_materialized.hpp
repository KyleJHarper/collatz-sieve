#pragma once


#include <unordered_map>
#include "concepts.hpp"
#include "equality_helper.hpp"
#include "node.hpp"
#include "binary_tree_math.hpp"
#include "binary_tree_coverage.hpp"
#include "binary_tree_options.hpp"
#include "binary_tree_types.hpp"
#include "stream_helpers.hpp"
#include <omp.h>
#include <tbb/parallel_sort.h>




//
// A perfect binary tree mapped to powers of two.  This creates a uniform distribution of nodes in the N/Z+ space (positive
// integers), which Collatz is concerned.
//
// This is the materialized version, which creates real Node objects and persists them.  When pruning is enabled, we will remove
// any nodes hitting "hwm", leaving only nodes that still need to be tested.
//
// Note: this is okay for small things or detailed inspection of the tree, but RAM explodes quickly.  Attempts were made to prune
// covered nodes and even parent levels, but it's a losing battle.  Consider the BinaryTreeImplicit implementation instead.
//
template<AnySupportedIntegral T>
class BinaryTreeMaterializedImpl {
    private:
    Node<T>* _root_node = nullptr;
    size_t _level_count = 0;
    std::unordered_map<size_t, std::vector<Node<T>*>> _level_map;
    std::unordered_map<size_t, BinaryTreeCoverage<T>> _coverage_map;
    std::vector<Node<T>*> _ancestors;
    bool _is_initialized = false;
    bool _is_pruning_hwm_nodes = BinaryTreeOptions{}.prune_hwm_nodes;
    bool _is_pruning_parent_levels = BinaryTreeOptions{}.prune_parent_levels;
    bool _is_preserving_ancestors = BinaryTreeOptions{}.preserve_ancestors;
    bool _is_verifying_non_hwm_nodes = BinaryTreeOptions{}.verify_non_hwm_nodes;
    const TreeTypeEnum _tree_type = TreeTypeEnum::MATERIALIZED;
    static constexpr BinaryTreeOptions DEFAULT_OPTS{};



    public:
    //
    // Constructors
    //
    BinaryTreeMaterializedImpl() = default;



    //
    // Destructor
    //
    ~BinaryTreeMaterializedImpl() {
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
        _is_pruning_hwm_nodes = opts.prune_hwm_nodes;
        _is_pruning_parent_levels = opts.prune_parent_levels;
        _is_preserving_ancestors = opts.preserve_ancestors;
        // Build the first level's root node, supporting both 0- and 1-based trees.
        _level_count = 1;
        _root_node = new Node<T>(BinaryTreeMath<T>::get_root_value());
        _level_map[_level_count].resize(1);
        _level_map[_level_count][0] = _root_node;
        _coverage_map[_level_count].set_covered(0);
        _coverage_map[_level_count].set_total(1);
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
        _level_map.clear();
        _level_count = 0;
        _ancestors.clear();
        _is_verifying_non_hwm_nodes = BinaryTreeOptions{}.verify_non_hwm_nodes;
        _is_pruning_hwm_nodes = BinaryTreeOptions{}.prune_hwm_nodes;
        _is_pruning_parent_levels = BinaryTreeOptions{}.prune_parent_levels;
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
    Node<T>* get_root_node_rw() { return _root_node; }
    const std::unordered_map<size_t, BinaryTreeCoverage<T>>& get_coverage_map() const { return _coverage_map; }
    std::unordered_map<size_t, BinaryTreeCoverage<T>>& get_coverage_map_rw() { return _coverage_map; }
    const std::vector<Node<T>*>& get_ancestors() const { return _ancestors; }
    std::vector<Node<T>*>& get_ancestors_rw() { return _ancestors; }
    bool is_preserving_ancestors() const { return _is_preserving_ancestors; }
    void set_is_preserving_ancestors(const bool value) { _is_preserving_ancestors = value; }
    bool is_verifying_non_hwm_nodes() const { return _is_verifying_non_hwm_nodes; }
    void disable_non_hwm_node_verification() { _is_verifying_non_hwm_nodes = false; }
    void enable_non_hwm_node_verification() { _is_verifying_non_hwm_nodes = true; }
    bool is_initialized() const { return _is_initialized; }
    void set_is_initialized(const bool value) { _is_initialized = value; }
    //
    // Materialized-Specific
    bool is_materialized() const { return true; }
    const std::unordered_map<size_t, std::vector<Node<T>*>>& get_level_map() const { return _level_map; }
    bool is_pruning_hwm_nodes() const { return _is_pruning_hwm_nodes; }
    bool is_pruning_parent_levels() const { return _is_pruning_parent_levels; }
    // Real Node Count
    // Returns the REAL node count in the _level_map.  Includes level 0 if tree is 1-based (no dead root).
    T real_node_count() const {
        T total = 0;
        for (size_t level = 1; level <= _level_count; level++) {
            total += _level_map.at(level).size();
        }
        return total;
    }



    //
    // Object Size
    // Deeply scan the object, all nodes, and so forth, tallying up the expect bytes used.  This won't match RSS because we can't
    // track alignment and such without massive headaches.
    //
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



    //
    // Equal
    // Compare all of the members and (meta) data.
    //
    // Returns true if they are equal in representation.  False otherwise.
    // Will explain what failed to *err if sent.
    //
    // Only implementation-specific checks go here.  Facade handles common.
    static bool st_equal(const BinaryTreeMaterializedImpl<T>& first, const BinaryTreeMaterializedImpl<T>& second, std::string* err = nullptr) {
        EqualityHelper eq(err);

        // Flags
        if (eq.unequal(first.is_pruning_hwm_nodes(), second.is_pruning_hwm_nodes())) {
            return eq.fail("Is pruning hwm nodes mismatch");
        }
        if (eq.unequal(first.is_pruning_parent_levels(), second.is_pruning_parent_levels())) {
            return eq.fail("Is pruning parent levels mismatch");
        }

        // Level Map
        if (eq.unequal(first.get_level_map().size(), second.get_level_map().size())) {
            return eq.fail("Level map count mismatch");
        }
        for (const auto& [level, f_nodes] : first.get_level_map()) {
            if (! second.get_level_map().contains(level)) {
                return eq.fail("First level map has level " + to_string_any(level) + ", but the second level map doesn't.");
            }
            const std::vector<Node<T>*>& s_nodes = second.get_level_map().at(level);
            if (eq.unequal(f_nodes.size(), s_nodes.size())) {
                return eq.fail("Node counts on level " + to_string_any(level) + " mismatch");
            }
            for (size_t i = 0; i < f_nodes.size(); i++) {
                if(! Node<T>::st_equal(f_nodes.at(i), s_nodes.at(i), err)) {
                    return false;
                }
            }
        }

        // Guess we made it here.  All good.
        return true;
    }
    //
    // Member helper.
    bool equal(const BinaryTreeMaterializedImpl<T>& second, std::string* err = nullptr) const {
        return BinaryTreeMaterializedImpl<T>::st_equal(*this, second, err);
    }



    //
    // Serialize
    // Serializes the implementation-specific details.  Facade handles common.
    //
    [[nodiscard]] bool serialize(std::ostream& out, std::string* err = nullptr) const {
        StreamHelper sh(nullptr, &out, err);
        sh.set_category("BinaryTreeMaterializedImpl");

        // Unique properties.
        if (! sh.serialize_bool(_is_pruning_hwm_nodes)) {
            return sh.fail("_is_pruning_hwm_nodes==" + std::to_string(_is_pruning_hwm_nodes));
        }
        if (! sh.serialize_bool(_is_pruning_parent_levels)) {
            return sh.fail("_is_pruning_parent_levels==" + std::to_string(_is_pruning_parent_levels));
        }

        // Level Map
        // Write how many levels we need to loop through later.
        //
        // Note: we do NOT serialize level 1, because it's only the root node, which is a common item.  We link it deserialize().
        uint64_t u64_level_map_count = get_level_map().size() - 1;
        if (! sh.serialize_integral(u64_level_map_count)) {
            return sh.fail("level_map_count==" + to_string_any(u64_level_map_count));
        }
        // Get the level numbers in order for guaranteed write order.
        std::vector<size_t> level_map_levels;
        level_map_levels.reserve(_level_map.size());
        for (const auto& [level, _] : _level_map) {
            if (level == 1) { continue; }
            level_map_levels.push_back(level);
        }
        std::sort(level_map_levels.begin(), level_map_levels.end());
        // Loop through each level we found.
        for (size_t level : level_map_levels) {
            const std::vector<Node<T>*>& nodes = _level_map.at(level);
            // Write the level number and then the number of nodes on it.
            uint64_t u64_level = static_cast<uint64_t>(level);
            if (! sh.serialize_integral(u64_level)) {
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



    //
    // Deserialize
    // Convert the "in" to our implementation-specific values and members.
    //
    [[nodiscard]] bool deserialize(std::istream& in, std::string* err = nullptr) {
        StreamHelper sh(&in, nullptr, err);
        sh.set_category("BinaryTreeMaterializedImpl");

        // Unique properties
        bool b_tmp;
        if (! sh.deserialize_bool(b_tmp)) {
            return sh.fail("couldn't read _is_pruning_hwm_nodes");
        }
        _is_pruning_hwm_nodes = b_tmp;
        if (! sh.deserialize_bool(b_tmp)) {
            return sh.fail("couldn't read _is_pruning_parent_levels");
        }
        _is_pruning_parent_levels = b_tmp;

        // Level map
        // Start by assigning the root node to level 1, because we don't export that in serialize().
        _level_map[1].resize(1);
        _level_map[1][0] = _root_node;
        // Now get the remaining levels, if any.
        uint64_t u64_level_map_count = 0;
        T parent_v;
        T hwm_ancestor_v;
        if (! sh.deserialize_integral(u64_level_map_count)) {
            return sh.fail("couldn't read level_map_count");
        }
        for (uint64_t i = 0; i < u64_level_map_count; i++) {
            uint64_t parent_index = 0;
            uint64_t u64_level = 0;
            if (! sh.deserialize_integral(u64_level)) {
                return sh.fail("couldn't read level for index i==" + to_string_any(i));
            }
            size_t level = static_cast<size_t>(u64_level);
            uint64_t u64_node_size = 0;
            if (! sh.deserialize_integral(u64_node_size)) {
                return sh.fail("couldn't read node size for level==" + to_string_any(level));
            }
            size_t node_size = static_cast<size_t>(u64_node_size);
            _level_map[level].reserve(node_size);
            for (uint64_t j = 0; j < u64_node_size; j++) {
                // Make a new heap-allocated node to deserialize into.
                Node<T>* new_node = new Node<T>();
                if (! new_node->deserialize(in, parent_v, hwm_ancestor_v, err)) {
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
                // If there's an ancestor, link it.  These are value-ordered in add_level(), so we can binary search.
                if (hwm_ancestor_v != 0) {
                    auto it = std::lower_bound(
                        _ancestors.begin(),
                        _ancestors.end(),
                        hwm_ancestor_v,
                        [](const Node<T>* ancestor, const T& val) {
                            return ancestor->get_value() < val;
                        }
                    );
                    if (it != _ancestors.end()) {
                        Node<T>* candidate = *it;
                        if (candidate->get_value() == hwm_ancestor_v) {
                            new_node->assign_hwm_ancestor(candidate);
                        }
                    }
                    if (new_node->get_hwm_ancestor() == nullptr) {
                        return sh.fail("couldn't find hwm ancestor for new node with value==" + to_string_any(new_node->get_value()));
                    }
                }
            }
        }

        // All good
        return true;
    }



    //
    // Add Level
    // Add a level to the tree.  We simply take the parent nodes and add two children with a steady step value.  We also calculate
    // the coverage for this level.
    //
    // When WHM pruning is enabled we trim all grandparents who meet HWM.  We need the parents to build the child level currently.
    // We assume the parent generation was already pruned, and we'll test children after we make them.
    //
    void add_level() {
        // Get the parent and child levels.  Then confirm the new one will fit.  Then bump count.
        size_t parent_level = _level_count;
        size_t child_level = _level_count + 1;
        _level_count++;

        // Build the step value.  Each level doubles the tree, so we need to respect T with GMP-size values.
        T step = BinaryTreeMath<T>::st_step(parent_level);

        // Calculate parent and child counts for looping and indexing.
        // Note: size_t is safe, because no one can fit 2^64 nodes in RAM.  Vectors require size_t for indexes.
        auto& parents = _level_map[parent_level];
        size_t parent_count = parents.size();
        size_t child_count = parent_count * 2;

        // Resize the vector to contain all children.  Otherwise assigning to indexes ([]) will fail.
        _level_map[child_level].resize(child_count);

        // Keep a record of covered or pruned values for coverage math after the loop.
        size_t covered_or_pruned = 0;

        // Create an external tracker for ancestors for OMP local threads to utilize by-ref.
        std::vector<std::vector<Node<T>*>> omp_local_ancestors_group(omp_get_max_threads());

        // Begin the critical section, but don't loop yet.
        std::exception_ptr eptr = nullptr;
        #pragma omp parallel reduction(+:covered_or_pruned) default(none) shared(parents, _level_map, step, child_level, parent_count, _is_pruning_hwm_nodes, _is_preserving_ancestors, omp_local_ancestors_group, _is_verifying_non_hwm_nodes, eptr)
        {
            try {
                // Use local child values.
                T child_value_1;
                T child_value_2;

                // Grab our personal ancestors vector to avoid locking.
                auto& my_ancestors = omp_local_ancestors_group[omp_get_thread_num()];

                // Track our own local High-Water Mark.  Since nodes are not sequentially ordered, we can only set this to first node - 1.
                T my_high_water_mark = BinaryTreeMath<T>::st_first_node_of_level(_level_count) - 1;

                // Now Loop
                #pragma omp for schedule(static)
                for(size_t parent_idx = 0; parent_idx < parent_count; parent_idx++) {
                    // Find the parent.
                    Node<T>* parent = parents[parent_idx];

                    // Compute the child values.  Avoid alloc() with GMP with arithmetic operators.
                    if constexpr(BuiltinIntegral<T>) {
                        child_value_1 = parent->get_value() + step;
                        child_value_2 = child_value_1 + step;
                    } else if constexpr(GMPIntegral<T>) {
                        mpz_add(child_value_1.get_mpz_t(), parent->get_value().get_mpz_t(), step.get_mpz_t());
                        mpz_add(child_value_2.get_mpz_t(), child_value_1.get_mpz_t(), step.get_mpz_t());
                    }

                    // Instaniate the children.
                    Node<T>* child_1 = new Node<T>(child_value_1, parent);
                    Node<T>* child_2 = new Node<T>(child_value_2, parent);

                    // Tally them.  Prune them if necessesary.  Otherwise add them to the map.
                    // -- Child 1
                    bool assign_to_map = true;
                    if (_is_preserving_ancestors && child_1->is_below_high_water_mark() && ! child_1->has_high_water_mark_ancestor()) {
                        my_ancestors.push_back(new Node<T>(child_1->get_value()));
                    }
                    if (_is_verifying_non_hwm_nodes) {
                        if (child_1->is_below_high_water_mark() == false && child_1->has_high_water_mark_ancestor() == false) {
                            if (Collatz<T>::st_verify(child_value_1, my_high_water_mark) == false) {
                                throw std::logic_error("Node value " + to_string_any(child_value_1) + " didn't verify.  How?");
                            }
                        }
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
                    // -- Child 2
                    assign_to_map = true;
                    if (_is_preserving_ancestors && child_2->is_below_high_water_mark() && ! child_2->has_high_water_mark_ancestor()) {
                        my_ancestors.push_back(new Node<T>(child_2->get_value()));
                    }
                    if (_is_verifying_non_hwm_nodes) {
                        if (child_2->is_below_high_water_mark() == false && child_2->has_high_water_mark_ancestor() == false) {
                            if (Collatz<T>::st_verify(child_value_2, my_high_water_mark) == false) {
                                throw std::logic_error("Node value " + to_string_any(child_value_2) + " didn't verify.  How?");
                            }
                        }
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
                if constexpr(BuiltinIntegral<T>) {
                    return a->get_value() < b->get_value();
                } else {
                    return mpz_cmp(a->get_value().get_mpz_t(), b->get_value().get_mpz_t()) < 0;
                }
            });
        }

        // When pruning nodes,remove any nullptr (pruned) children from the vector to keep counts accurate on next loop.
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
            // When it's level 1, we need to clear up _root too so it doesn't double-free later.
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
        // descendents were purged, but still count toward coverage.  Luckily, by shrinking the vector above, we can rely on
        // .size() to tell us how many are *not* covered, which means covered = total - .size().
        T total = step * 2;
        T covered = covered_or_pruned;
        if (_is_pruning_hwm_nodes) {
            covered = total - _level_map[child_level].size();
        }
        _coverage_map[child_level] = BinaryTreeCoverage<T>(covered, total);
    }

};
