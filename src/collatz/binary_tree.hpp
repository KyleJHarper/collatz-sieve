#pragma once

//
// Contains both the BinaryTreeMaterialized and BinaryTreeImplicit implementations, along with the BinaryTree facade.
//
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <gmp.h>
#include <gmpxx.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <omp.h>
#include "concepts.hpp"
#include "binary_tree_backend_interface.hpp"
#include "node.hpp"



//
// Options Package for BinaryTree
//
enum class BinaryTreeType {
    MATERIALIZED = 1,
    IMPLICIT = 2,
};
struct BinaryTreeOptions {
    bool prune_hwm_nodes = false;
    bool prune_parent_levels = false;
    bool preserve_ancestors = false;
    BinaryTreeType tree_type = BinaryTreeType::MATERIALIZED;
};



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
class BinaryTreeMaterialized : public IBinaryTreeBackend<T> {
    private:
    Node<T> *_root_node = nullptr;
    size_t _level_count = 0;
    std::unordered_map<size_t, std::vector<Node<T>*>> _level_map;
    std::unordered_map<size_t, BinaryTreeCoverage<T>> _coverage_map;
    std::vector<Node<T>*> _ancestors;
    bool _is_initialized = false;
    bool _is_pruning_hwm_nodes = false;
    bool _is_pruning_parent_levels = false;
    bool _preserve_ancestors = false;

    public:
    //
    // Constructors
    //
    static constexpr BinaryTreeOptions DEFAULT_OPTS{};
    BinaryTreeMaterialized(const BinaryTreeOptions& opts = DEFAULT_OPTS) {
        _is_pruning_hwm_nodes = opts.prune_hwm_nodes;
        _is_pruning_parent_levels = opts.prune_parent_levels;
        _preserve_ancestors = opts.preserve_ancestors;
    }



    //
    // Destructor
    //
    ~BinaryTreeMaterialized() {
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
    void init(size_t levels) override {
        // Sanity check.  T must support requested tree size.
        this->assert_level_will_fit(levels);
        // Reset object if necessary.
        if(_is_initialized) { reset(); }
        _is_initialized = true;
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
    void reset() override {
        _is_initialized = false;
        _coverage_map.clear();
        _level_map.clear();
        _level_count = 0;
        _ancestors.clear();
        _is_pruning_hwm_nodes = BinaryTreeOptions{}.prune_hwm_nodes;
        _is_pruning_parent_levels = BinaryTreeOptions{}.prune_parent_levels;
        _preserve_ancestors = BinaryTreeOptions{}.preserve_ancestors;
    }



    //
    // Getters and Accessors
    //
    size_t get_level_count() const override { return _level_count; }
    Node<T>* get_root_node() const override { return _root_node; }
    const std::unordered_map<size_t, std::vector<Node<T>*>>& get_level_map() const override { return _level_map; }
    const std::unordered_map<size_t, BinaryTreeCoverage<T>>& get_coverage_map() const override { return _coverage_map; }
    const std::vector<Node<T>*> get_ancestors() const override { return _ancestors; }
    bool is_pruning_hwm_nodes() const override { return _is_pruning_hwm_nodes; }
    bool is_pruning_parent_levels() const override { return _is_pruning_parent_levels; }
    const std::unordered_map<size_t, std::vector<Interval<T>>>& get_covered_intervals() const override {
        throw std::logic_error("BinaryTreeMaterialized has no coverage interval property.");
    }


    //
    // Node Count
    // Returns the REAL node count in the _level_map.  Includes level 0 if tree is 1-based (no dead root).
    //
    T node_count() const override {
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
    size_t deep_size() const override {
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
    // Add Level
    // Add a level to the tree.  We simply take the parent nodes and add two children with a steady step value.  We also calculate
    // the coverage for this level.
    //
    // When WHM pruning is enabled we trim all grandparents who meet HWM.  We need the parents to build the child level currently.
    // We assume the parent generation was already pruned, and we'll test children after we make them.
    //
    void add_level() override {
        // Get the parent and child levels.  Then confirm the new one will fit.  Then bump count.
        size_t parent_level = _level_count;
        size_t child_level = _level_count + 1;
        this->assert_level_will_fit(child_level);
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

        // Create child value objects.  OMP is okay, because the thread_local keeps them separated.  Prevents GMP reallocs().
        static thread_local T child_value_1;
        static thread_local T child_value_2;

        // Keep a record of covered or pruned values for coverage math after the loop.
        size_t covered_or_pruned = 0;

        // Create an external tracker for ancestors for OMP local threads to utilize by-ref.
        std::vector<std::vector<Node<T>*>> omp_local_ancestors_group(omp_get_max_threads());

        // Begin the critical section, but don't loop yet.
        std::exception_ptr eptr = nullptr;
        #pragma omp parallel reduction(+:covered_or_pruned) default(none) shared(parents, _level_map, step, child_level, parent_count, _is_pruning_hwm_nodes, _preserve_ancestors, omp_local_ancestors_group, eptr)
        {
            try {
                // Grab our personal ancestors vector to avoid locking.
                auto& my_ancestors = omp_local_ancestors_group[omp_get_thread_num()];

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
                    if (_preserve_ancestors && child_1->is_below_high_water_mark() && ! child_1->has_high_water_mark_ancestor()) {
                        my_ancestors.push_back(new Node<T>(child_1->get_value()));
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
                    if (_preserve_ancestors && child_2->is_below_high_water_mark() && ! child_2->has_high_water_mark_ancestor()) {
                        my_ancestors.push_back(new Node<T>(child_2->get_value()));
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
        if (_preserve_ancestors) {
            for (std::vector<Node<T>*>& omp_ancestors : omp_local_ancestors_group) {
                _ancestors.insert(_ancestors.end(), omp_ancestors.begin(), omp_ancestors.end());
            }
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



    //
    // Generate Node At
    // Generate any Node based on its level and position.  It will not be part of any tree.
    // Throws errors when you ask for invalid positions in a node.
    //
    static Node<T>* st_generate_node_at(size_t level, T position) {
        // Calculate the maximum position and enforce the rules.  We will need the first node's value too.
        T max_position = BinaryTreeMath<T>::st_node_count_of_level(level);
        if(position > max_position) {
            throw std::out_of_range("Cannot ask for a position outside of a level's limits.");
        }
        if(position < 1) {
            throw std::out_of_range("You cannot specify position 0 or lower (negative).  Positions start at 1 (leftmost).");
        }
        T node_value = BinaryTreeMath<T>::st_node_value_by_position_and_level(position, level);
        Node<T>* node = new Node<T>(node_value);
        return node;
    }
};





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
    std::unordered_map<size_t, std::vector<Interval<T>>> _covered_intervals;
    std::unordered_map<size_t, BinaryTreeCoverage<T>> _coverage_map;
    std::vector<Node<T>*> _ancestors;
    bool _is_initialized = false;
    bool _preserve_ancestors = false;

    public:
    //
    // Constructors
    //
    static constexpr BinaryTreeOptions DEFAULT_OPTS{};
    BinaryTreeImplicit(const BinaryTreeOptions& opts = DEFAULT_OPTS) {
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
        _covered_intervals.clear();
        _level_count = 0;
        _ancestors.clear();
        _preserve_ancestors = BinaryTreeOptions{}.preserve_ancestors;
    }



    //
    // Getters and Accessors
    //
    size_t get_level_count() const override { return _level_count; }
    Node<T>* get_root_node() const override { return _root_node; }
    const std::unordered_map<size_t, std::vector<Interval<T>>>& get_covered_intervals() const override { return _covered_intervals; }
    const std::unordered_map<size_t, BinaryTreeCoverage<T>>& get_coverage_map() const override { return _coverage_map; }
    const std::vector<Node<T>*> get_ancestors() const override { return _ancestors; }
    const std::unordered_map<size_t, std::vector<Node<T>*>>& get_level_map() const override {
        throw std::logic_error("Implicit trees do not have a level map");
    }
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

        // Account for _covered_intervals
        total += sizeof(_covered_intervals);
        total += _covered_intervals.bucket_count() * sizeof(void*);
        for (const auto& [level, intervals] : _covered_intervals) {
            total += sizeof(level);
            total += sizeof(intervals);
            total += intervals.capacity() * sizeof(Interval<T>);
        }

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
    // Get Uncovered Intervals
    // Returns the intervals which complement the _covered_intervals we've found.
    //
    // In other words: return the ranges of nodes still to-check for a given level.
    //
    std::vector<Interval<T>> get_uncovered_intervals(size_t requested_level) const {
        // Determine the max level we can pull from for extrapolation.
        size_t max_coverage_level = requested_level <= _level_count ? requested_level : _level_count;

        // Create the results variable and setup our max width and cursor to follow.
        std::vector<Interval<T>> uncovered_intervals;
        T max_position = BinaryTreeMath<T>::st_node_count_of_level(requested_level);

        // If no intervals exist, try dropping down a level until we hit zero.
        while (_covered_intervals.find(max_coverage_level) == _covered_intervals.end() || _covered_intervals.at(max_coverage_level).empty()) {
            max_coverage_level--;
            if (max_coverage_level == 0) {
                uncovered_intervals.push_back({1, max_position});
                return uncovered_intervals;
            }
        }

        // We must have at least some coverage intervals if we're here.
        const std::vector<Interval<T>>& latest_covered_intervals = _covered_intervals.at(max_coverage_level);

        // Sort them, in case.
        // The add_level() always sorts before storing.  Can skip here.

        // Now build the complement of intervals.
        T prev_end = 0;
        T scale = T(1) << (requested_level - max_coverage_level);
        for (const Interval<T>& covered_interval : latest_covered_intervals) {
            T scaled_start = (covered_interval.start - 1) * scale + 1;
            T scaled_end = covered_interval.end * scale;
            if (scaled_start > prev_end + 1) {
                uncovered_intervals.push_back({T(prev_end + 1), T(scaled_start - 1)});
            }
            prev_end = scaled_end;
        }

        // Cover any gap from the last covered to the end of the level.
        if (prev_end < max_position) {
            uncovered_intervals.push_back({T(prev_end + 1), max_position});
        }

        return uncovered_intervals;
    }



    //
    // Partition Workers
    // Takes a vector of intervals and a partition count and returns a new vector-of-vectors with intervals with their sizes
    // balanced nearly evenly.  This will split a partition, if necessary, to avoid heavy imbalance.
    //
    std::vector<std::vector<Interval<T>>> partition_intervals(const std::vector<Interval<T>>& intervals, size_t partition_count) {
        // Build the new vector of vectors with the correct size.
        std::vector<std::vector<Interval<T>>> partitions;
        partitions.resize(partition_count);

        // Guard against zero or null sets.
        if (partition_count == 0 || intervals.empty()) {
            return partitions;
        }

        // Calculate the total size.
        T total_size = 0;
        for (const Interval<T>& interval : intervals) {
            total_size += interval.size();
        }

        // Set the partition size.  Increment by 1 to cover integral truncation.
        T partition_size = (total_size / partition_count) + 1;
        T current_size = 0;
        size_t partition_id = 0;

        // Loop through intervals again, assigning replacements and/or breaking them apart.
        for (const Interval<T>& interval : intervals) {
            Interval<T> tmp;
            T current_start = interval.start;
            while (current_start <= interval.end) {
                // Start by assuming our tmp can handle the whole space.
                tmp.start = current_start;
                tmp.end = interval.end;
                // Determine how many places we can take.
                T remaining_partition_size = partition_size - current_size;
                T can_take = remaining_partition_size <= tmp.size() ? remaining_partition_size : tmp.size();
                // Change tmp's start and end to match our limits.
                tmp.start = current_start;
                tmp.end = current_start + can_take - 1;  // -1 Because it's inclusive.
                // Add it to the current partition.  Update count.  Increment ID if needed.
                partitions[partition_id].push_back(tmp);
                current_size += tmp.size();
                if (current_size >= partition_size) {
                    partition_id++;
                    current_size = 0;
                }
                // Move the current_start forward for the next condition check.
                current_start = tmp.end + 1;
            }
        }

        // All done.
        return partitions;
    }



    //
    // Add Level
    // Add a level to the tree, which simply means scan the next level for new covered sections (HWM ancestors).
    //
    void add_level() override {
        // Confirm the new level will fit, then bump the count.
        this->assert_level_will_fit(_level_count + 1);
        _level_count++;

        // Carry the previous level's covered intervals forward by scaling them.
        for (Interval<T>& covered_interval : _covered_intervals[_level_count - 1]) {
            Interval<T> scaled_interval = {.start=T(covered_interval.start * 2 - 1), .end=T(covered_interval.end * 2)};
            _covered_intervals[_level_count].push_back(scaled_interval);
        }

        // Grab the uncovered intervals.
        std::vector<Interval<T>> uncovered_intervals = get_uncovered_intervals(_level_count);

        // Partition them to create uniform workloads for OMP.
        size_t thread_count = omp_get_max_threads();
        std::vector<std::vector<Interval<T>>> partitions = partition_intervals(uncovered_intervals, thread_count);

        // Create an external tracker for new ancestors (which inherently are the new intervals).
        std::vector<std::vector<Node<T>>> omp_local_ancestors_group(thread_count);

        // Begin the critical section, but don't loop yet.
        std::exception_ptr eptr = nullptr;
        #pragma omp parallel default(none) shared(omp_local_ancestors_group, partitions, eptr)
        {
            try {
                // Grab our personal ancestors and parittion vectors to avoid locking.
                size_t my_thread_id = omp_get_thread_num();
                auto& my_ancestors = omp_local_ancestors_group[my_thread_id];
                auto& my_partition = partitions[my_thread_id];

                // Now loop without triggering OMP again, because the work is already divided by partitions.
                Node<T> tmp_node;
                T value = 0;
                for(const Interval<T>& interval : my_partition) {
                    for(T position = interval.start; position <= interval.end; position++) {
                        value = BinaryTreeMath<T>::st_node_value_by_position_and_level(position, _level_count);
                        tmp_node.init(value);
                        if (tmp_node.is_below_high_water_mark()) {
                            my_ancestors.push_back(tmp_node);
                        }
                    }
                }
            } catch (...) {
                #pragma omp critical
                {
                    if (!eptr) {
                        eptr = std::current_exception();
                    }
                }
            }
        }

        // Rethrow errors.
        if (eptr != nullptr) {
            std::rethrow_exception(eptr);
        }

        // Convert ancestors into intervals and optionally persist them.
        // Additionally, calculate the Coverage object since we're looping through intervals anyway.
        static thread_local T position = 0;
        for (std::vector<Node<T>>& new_ancestors : omp_local_ancestors_group) {
            for (Node<T>& node : new_ancestors) {
                position = node.get_position();
                Interval<T> new_interval = {.start = position, .end = position};
                _covered_intervals[_level_count].push_back(new_interval);
                if (_preserve_ancestors) {
                    _ancestors.push_back(new Node<T>(node));
                }
            }
        }
        // Sort the covered intervals so they work correctly later.  Since start always equals end, it's trivial.
        std::sort(
            _covered_intervals[_level_count].begin(),
            _covered_intervals[_level_count].end(),
            [](const Interval<T>& a, const Interval<T>& b) { return a.start < b.start; }
        );
        // Persist the coverage to our new level.  It's just a sum of the covered size() values.
        T total = BinaryTreeMath<T>::st_node_count_of_level(_level_count);
        T covered = 0;
        for (Interval<T>& covered_interval : _covered_intervals[_level_count]) {
            covered += covered_interval.size();
        }
        _coverage_map[_level_count] = BinaryTreeCoverage<T>(covered, total);
    }

};




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
    const std::unordered_map<size_t, std::vector<Interval<T>>>& get_covered_intervals() const {
        assert_implicit("get_covered_intervals");
        return _impl->get_covered_intervals();
    }

};
