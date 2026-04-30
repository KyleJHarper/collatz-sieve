#pragma once

#include "binary_tree_materialized.hpp"
#include "binary_tree_implicit.hpp"
#include "binary_tree_types.hpp"
#include "concepts.hpp"
#include <stdexcept>
#include <type_traits>
#include "stream_helper.hpp"
#include <fstream>
#include "equality_helper.hpp"
#include <tbb/parallel_sort.h>
#include "abi_helpers.hpp"
#include "zstd_compress.hpp"
#include "zstd_decompress.hpp"
#include <cstdio>
#include <unistd.h>




/**
* @brief A truthful concept when the `BinaryTreeMaterializedImpl` is selected in the `BinaryTree` facade.
*/
template<typename TreeType>
concept IsMaterializedTree = requires(const TreeType& tree_type) { tree_type.is_materialized(); };



/**
* @brief A truthful concept when the `BinaryTreeImplicitImpl` is selected in the `BinaryTree` facade.
*/
template<typename TreeType>
concept IsImplicitTree = requires(const TreeType& tree_type) { tree_type.is_implicit(); };



/**
* @struct BinaryTreeFileHeader
* @brief A set of const values for exported trees (saved to disk/stream).  Namely a magic value and version number.
*/
struct BinaryTreeFileHeader {
    /// @brief The size of the magic bytes.
    static const uint32_t MAGIC_SIZE = 10;
    /// @brief Magic bytes.
    const char magic[MAGIC_SIZE] = {'H', 'a', 'r', 'p', 'e', 'r', 'T', 'r', 'e', 'e'}; // "HarperTree"
    /// @brief Version number.  Must change whenever serialization/deserialization changes happen.
    const uint32_t version = 1;
};




/**
* @class BinaryTree
* @brief The BinaryTree facade which will apply Harper's sieve of High-Water Mark nodes.
*
* This static facade will build a binary tree using the child values (labels) defined in Harper's research, which will find and
* remove any nodes whose FG-chain reaches the High-Water Mark, eliminating subtrees.
*
* Users may choose the `BinaryTreeMaterializedImpl` or the `BinaryTreeImplicitImpl`.
*
* The `BinaryTreeMaterializedImpl` is a true, memory-backed tree with nodes connected via edges (pointers).  It is extremely heavy
* in RAM, but its saving grace is that it's a true binary tree which users can walk and analyze.  If pruning is enabled, nodes are
* removed according to the pruning policy, leaving what is technically a directed acyclic graph (DAG).
*
* The `BinaryTreeImplicitImpl` is a mathematically equivalent tree, and is the default as of version 4.0.0.  It uses significantly
* less memory by foregoing nodes in favor of a `NodeBitmap` which tracks the uncovered positions: the leaf node positions of the
* tree which haven't been pruned by a High-Water Mark ancestor yet.
*
* @tparam T Any supported integral (see concepts.hpp).
* @tparam TreeType The implementation type (with type T nested) to use for the `_impl` member.
*/
template<AnySupportedIntegral T, typename TreeType = BinaryTreeImplicitImpl<T>>
class BinaryTree {
    private:
    /// @brief The static implementation object.  See `init()` for reuse.
    TreeType _impl;



    public:
    /// @name Lifecycle Management
    /// @{

    /**
    * @brief Default constructor.  No levels added.
    */
    BinaryTree() = default;



    /**
    * @brief Constructor which will call `init()` and set custom options, if specified.
    * @note Not all options apply to all implementations.  Unused options do not emit warnings.
    * @param levels The number of levels to build, inclusive.
    * @param opts The package of `BinaryTreeOptions` to apply, if any.
    */
    BinaryTree(level_t levels, const BinaryTreeOptions& opts = BinaryTreeOptions{}) {
        init(levels, opts);
    }



    /**
    * @brief Destruction of the facade is simple.  Implementations in `_impl` have custom destruction, as needed.
    */
    ~BinaryTree() = default;



    /**
    * @brief Initializes the tree, rebuilding it if necessary.
    *
    * Asserts the requested levels will fit via `assert_level_will_fit()` which is mostly based on type `T`.  Then calls the
    * `_impl.init()` which will `reset()` and `init()` itself.  You do not need to call `reset()` manually.
    *
    * @note Not all options apply to all implementations.  Unused options do not emit warnings.
    * @param levels The number of levels to build, inclusive.
    * @param opts The package of `BinaryTreeOptions` to apply, if any.
    */
    void init(level_t levels, const BinaryTreeOptions& opts = BinaryTreeOptions{}) {
        // Sanity check.  T must support requested tree size.
        assert_level_will_fit(levels);
        _impl.init(levels, opts);
    }



    /**
    * @brief Resets implementation members by calling `_impl.reset()`.  This object remains unaffected.
    */
    void reset() {
        _impl.reset();
    }

    /// @}



    /// @name Accessors Common to All Trees
    /// @{

    /// @brief Return a readonly reference to the implementation.
    const TreeType& get_impl() const { return _impl; }

    /// @brief Get the tree type of this instance.
    TreeTypeEnum get_tree_type() const { return _impl.get_tree_type(); }

    /// @brief Get the level count.
    level_t get_level_count() const { return _impl.get_level_count(); }

    /// @brief Get a readonly pointer to the root node.
    Node<T>* get_root_node() const { return _impl.get_root_node(); }

    /// @brief Get a readonly reference to the coverage map.
    const std::unordered_map<level_t, BinaryTreeCoverage<T>>& get_coverage_map() const { return _impl.get_coverage_map(); }

    /// @brief Get a readonly reference to the ancestors.
    const std::vector<Node<T>*>& get_ancestors() const { return _impl.get_ancestors(); }

    /// @brief Get a flag whether this tree is preserving ancestors.
    bool is_preserving_ancestors() const { return _impl.is_preserving_ancestors(); }

    /// @brief Get a flag whether this tree is verifying non-HWM nodes.
    bool is_verifying_non_hwm_nodes() const { return _impl.is_verifying_non_hwm_nodes(); }
    /// @brief Explicitly disable non-HWM node verification.
    void disable_non_hwm_node_verification() { _impl.disable_non_hwm_node_verification(); }
    /// @brief Explicitly enable non-HWM node verification.
    void enable_non_hwm_node_verification() { _impl.enable_non_hwm_node_verification(); }

    /// @brief Get a flag whether this tree is initialized.
    bool is_initialized() const { return _impl.is_initialized(); }

    /// @brief Get the number of nodes for a tree of this size.
    /// @note This uses `BinaryTreeMath::st_node_count_of_tree()`.  It does not walk the actual `_impl` or read it in any way.
    T node_count() const { return BinaryTreeMath<T>::st_node_count_of_tree(_impl.get_level_count()); }

    /// @}



    /// @name Materialized-Only Accessors
    /// @{

    /// @brief Attaches `is_materialized()` if using the `BinaryTreeMaterializedImpl`.
    bool is_materialized() const requires IsMaterializedTree<TreeType> { return true; }

    /// @brief Get a reference to the level map, which contains all of the `Node` objects.
    /// @note Nodes are left-to-right by position, not node value.
    const std::unordered_map<level_t, std::vector<Node<T>*>>& get_level_map() const requires IsMaterializedTree<TreeType> {
        return _impl.get_level_map();
    }

    /// @brief Get a flag whether this tree is pruning High-Water Mark nodes.
    bool is_pruning_hwm_nodes() const requires IsMaterializedTree<TreeType> {
        return _impl.is_pruning_hwm_nodes();
    }

    /// @brief Get a flag whether this tree is pruning parent levels, leaving only the last one (leaf nodes).
    bool is_pruning_parent_levels() const requires IsMaterializedTree<TreeType> {
        return _impl.is_pruning_parent_levels();
    }

    /// @brief Returns the REAL node count in the _level_map.
    T real_node_count() const requires IsMaterializedTree<TreeType> {
        return _impl.real_node_count();
    }

    /// @}



    /// @name Implicity-Only Accessors
    /// @{

    /// @brief Attaches `is_implicit()` if using the `BinaryTreeImplicitImpl`.
    bool is_implicit() const requires IsImplicitTree<TreeType> { return true; }

    /// @brief Get a reference to the uncovered positions, which are the node positions not covered by a High-Water Mark ancestor.
    /// @note These are left-to-right positions, not node values.
    const NodeBitmap<T>& get_uncovered_positions() const requires IsImplicitTree<TreeType> {
        return _impl.get_uncovered_positions();
    }

    /// @}



    /**
    * @brief Assert the level requested will fit within the limits of `T` used.
    * @warning This is an assertion, not a boolean.  It will throw if there's an error.
    * @param level The level being added.
    */
    void assert_level_will_fit(level_t level) const {
        if (! BinaryTreeMath<T>::st_level_will_fit(level)) {
            size_t bits = sizeof(T) * 8;
            level_t max_level = bits - 1 - (std::is_signed_v<T> ? 1 : 0);
            std::string msg = "Cannot build a BinaryTree with ";
            msg += to_string_any(level) + " levels and type '" + typeid(T).name() + "' with ";
            msg += to_string_any(bits) + " bits. A Collatz sequence will overflow.";
            msg += " Max level for this type is " + to_string_any(max_level) + ".";
            throw std::out_of_range(msg);
        }
    }



    /**
    * @brief Assert non High-Water Mark verification is enabled when necessary.
    *
    * If verification is disabled (default) and the tree level exceeds the max known, fail.  It might seen annoying, but the API
    * refuses to allow people to build trees beyond empirically tested bounardaries unless they are willing to verify non HWM nodes
    * themsleves.  The limit is defined by `CollatzConstants::LARGEST_EMPIRICALLY_TESTED_LEVEL`.
    *
    * \par Deeper Explanation
    *
    * When building a tree, the nodes which don't meet HWM (directly or by ancestor) need to be checked.  For example, the
    * number `14` is below the HWM because of the `G` step on node `2`.  Its value becomes `7`.  That node isn't a HWM node,
    * descendant of one, nor has it been verified by the tree.  This leaves pockets of nodes that aren't verified, which means the
    * overall High-Water Mark of the tree isn't valid.
    *
    * To compensate, verification can be activated via the `BinaryTreeOptions` during `init()`, or by calling
    * `enable_non_hwm_node_verification()` on an instantiated tree.  However, external work by
    * [David Barina](https://link.springer.com/article/10.1007/s11227-025-07337-0) has empirically verified up to `2^71`, which is
    * the first node on level `72` of a tree.  Ergo, nodes up through level `71` are verified and do not need re-verification.
    *
    * @warning This is an assertion, not a boolean.  It will throw if verification is disabled and level count grows beyond limits.
    * @param level The level being added.
    * @param is_verifying_non_hwm_nodes Flag indicating if verification is enabled or not.
    */
    void assert_level_verification(level_t level, bool is_verifying_non_hwm_nodes) {
        if (is_verifying_non_hwm_nodes == false && level >= CollatzConstants::LARGEST_EMPIRICALLY_TESTED_LEVEL + 1) {
            std::string msg= "Tree has reached max level of previously verified space: "
            + to_string_any(level)
            + ".  You have disabled runtime verification, which means this and future levels are NOT fully verified!"
            + "  Refusing to build level.";
            throw std::runtime_error(msg);
        }
    }



    /**
    * @brief Adds a level to the tree.  Details are implementation-specific.
    *
    * This facade will assert the level fits, assert verification is correctly handled/covered, and then call the implementation's
    * `add_level()` method.
    */
    void add_level() {
        level_t next_level = _impl.get_level_count() + 1;
        assert_level_will_fit(next_level);
        assert_level_verification(next_level, _impl.is_verifying_non_hwm_nodes());
        _impl.add_level();
    }



    /**
    * @brief Deeply scan the object and report its size.
    */
    size_t deep_size() const { return _impl.deep_size(); }



    /**
    * @brief Compare two trees of any type and return true if identical.
    *
    * This is the facade method which will check the common items and then call the implementation for its specific checks.
    *
    * This function checks:
    *   1. Root nodes.
    *   2. Level count.
    *   3. The is_preserving_ancestors flag.
    *   4. The is_verifying_non_hwm_nodes flag.
    *   5. The is_initialized flag.
    *   6. Coverage map.
    *   7. Ancestors (if enabled).
    *   8. Implementation-specific checks from `TreeType::st_equal()`.
    *
    * @param first The first tree to compare.
    * @param second The second tree to compare.
    * @param err Pointer to a string where inequality or error messages are stored.
    * @return True if equal, false otherwise.
    */
    static bool st_equal(const BinaryTree<T, TreeType>& first, const BinaryTree<T, TreeType>& second, std::string* err = nullptr) {
        // Common to all trees.
        EqualityHelper eq(err);
        eq.set_category("BinaryTree");

        // Root node
        if (! Node<T>::st_equal(first.get_root_node(), second.get_root_node(), err)) {
            return eq.fail("Root nodes don't match.");
        }

        // Level count
        if (eq.unequal(first.get_level_count(), second.get_level_count())) {
            return eq.fail("Level counts differ");
        }

        // Flags
        if (eq.unequal(first.is_preserving_ancestors(), second.is_preserving_ancestors())) {
            return eq.fail("Is preserving ancestors mismatch");
        }
        if (eq.unequal(first.is_verifying_non_hwm_nodes(), second.is_verifying_non_hwm_nodes())) {
            return eq.fail("Is verifying hwm nodes mismatch");
        }
        if (eq.unequal(first.is_initialized(), second.is_initialized())) {
            return eq.fail("Is initialized mismatch");
        }

        // Coverage
        // Ensure the same size because it's cheap.
        if (eq.unequal(first.get_coverage_map().size(), second.get_coverage_map().size())) {
            return eq.fail("Coverage map size differs");
        }
        // Compare each level+coverage combo.
        for (const auto& [level, f_coverage] : first.get_coverage_map()) {
            if (! second.get_coverage_map().contains(level)) {
                return eq.fail("Coverage map in 'first' has level " + to_string_any(level) + " but 'second doesn't.");
            }
            const BinaryTreeCoverage<T>& s_coverage = second.get_coverage_map().at(level);
            if (eq.unequal(f_coverage.get_covered(), s_coverage.get_covered())) {
                return eq.fail("Coverage get_covered() mismatch");
            }
            if (eq.unequal(f_coverage.get_total(), s_coverage.get_total())) {
                return eq.fail("Coverage get_total() mismatch");
            }
        }

        // Ancestors
        // For this to work, they must be a consistent order, which the impls guarantee on add_level();
        const std::vector<Node<T>*>& f_ancestors = first.get_ancestors();
        const std::vector<Node<T>*>& s_ancestors = second.get_ancestors();
        if (eq.unequal(f_ancestors.size(), s_ancestors.size())) {
            return eq.fail("Ancestor count mismatch");
        }
        for (size_t i = 0; i < f_ancestors.size(); i++) {
            if (! Node<T>::st_equal(f_ancestors.at(i), s_ancestors.at(i), err)) {
                return eq.fail("Ancestor node mismatch");
            }
        }

        // Implementation-specific checks.
        if (! TreeType::st_equal(first.get_impl(), second.get_impl(), err)) {
            return eq.fail("Implementation-specific details mismatched");
        }

        // Everything is okay.
        return true;
    }



    /**
    * @brief Member helper which uses `st_equal()` under the hood.
    */
    bool equal(const BinaryTree<T, TreeType>& second, std::string* err = nullptr) const {
        return BinaryTree<T, TreeType>::st_equal(*this, second, err);
    }



    /**
    * @brief Serialize the implicit tree specifics of this object for export.
    *
    * Serialization happens in this order:
    *   1. Magic string and version from `BinaryTreeFileHeader`.
    *   2. Common metadata.
    *   3. Root node.
    *   4. Coverage.
    *   5. Ancestors.
    *   6. Implementation-specific details.
    *
    * @note This method does not throw.
    * @param out The stream to write data to.
    * @param err Pointer to a string where errors, if any, are written.
    * @return A boolean indicating success or failure.  Do not discard.
    */
    [[nodiscard]] bool serialize(std::ostream& out, std::string* err = nullptr) const {
        StreamHelper sh(nullptr, &out, err);
        sh.set_category("BinaryTree");

        // Header
        const BinaryTreeFileHeader header;
        if (! sh.write_bytes(&(header.magic), header.MAGIC_SIZE)) {
            return sh.fail("header magic string==" + std::string(header.magic));
        }
        if (! sh.serialize_integral(header.version)) {
            return sh.fail("header version==" + to_string_any(header.version));
        }

        // Common Metadata
        uint8_t tree_type = IsImplicitTree<TreeType> ? TreeTypeEnum::IMPLICIT : TreeTypeEnum::MATERIALIZED;
        uint32_t level_count = get_level_count();
        bool b_is_preserving_ancestors = is_preserving_ancestors();
        bool b_is_verifying_non_hwm_nodes = is_verifying_non_hwm_nodes();
        if (! sh.serialize_integral(tree_type)) {
            return sh.fail("tree_type==" + to_string_any(tree_type));
        }
        if (! sh.serialize_integral(level_count)) {
            return sh.fail("level_count==" + to_string_any(level_count));
        }
        if (! sh.serialize_bool(b_is_preserving_ancestors)) {
            return sh.fail("b_is_preserving_ancestors==" + std::to_string(b_is_preserving_ancestors));
        }
        if (! sh.serialize_bool(b_is_verifying_non_hwm_nodes)) {
            return sh.fail("b_is_verifying_non_hwm_nodes==" + std::to_string(b_is_verifying_non_hwm_nodes));
        }
        // The is_initialized must be true, so we ignore it.

        // Root node
        bool b_has_root_node = get_root_node() != nullptr;
        if (! sh.serialize_bool(b_has_root_node)) {
            return sh.fail("b_has_root_node");
        }
        if (b_has_root_node) {
            if (! get_root_node()->serialize(out, err)) {
                return sh.fail("root node");
            }
        }

        // Coverage
        // Write out how many entries exist so we know how many loops to make.
        const std::unordered_map<level_t, BinaryTreeCoverage<T>>& coverage_map = _impl.get_coverage_map();
        uint64_t u64_entry_count = coverage_map.size();
        if (! sh.serialize_integral(u64_entry_count)) {
            return sh.fail("coverage entry_count==" + to_string_any(u64_entry_count));
        }
        // Loop through them, emitting the level and then the coverage itself.
        // Sort the levels to ensure consistent output.  Nodes themselves are always ordered consistently.
        std::vector<level_t> coverage_levels;
        coverage_levels.reserve(coverage_map.size());
        for (const auto& [level, _] : coverage_map) {
            coverage_levels.push_back(level);
        }
        std::sort(coverage_levels.begin(), coverage_levels.end());
        for (level_t level : coverage_levels) {
            uint64_t u64_level = static_cast<uint64_t>(level);
            const BinaryTreeCoverage<T>& coverage = coverage_map.at(level);
            if (! sh.serialize_integral(u64_level)) {
                return sh.fail("u64_level==" + to_string_any(u64_level));
            }
            if (! coverage.serialize(out, err)) {
                return sh.fail("coverage on level " + to_string_any(level));
            }
        }

        // Ancestors
        // These are new-placement constructed (no children).  Just store the value.
        // First, write out how many there are.
        const std::vector<Node<T>*>& ancestors = get_ancestors();
        uint64_t u64_size = static_cast<uint64_t>(ancestors.size());
        if (! sh.serialize_integral(u64_size)) {
            return sh.fail("get_ancestors().size()==" + to_string_any(u64_size));
        }
        // Now loop through them.
        for (const Node<T>* ancestor : ancestors) {
            if (! ancestor->serialize(out, err)) {
                return sh.fail("ancestor with value==" + to_string_any(ancestor->get_value()));
            }
        }

        // Implementation-Specific Hook
        if (! _impl.serialize(out, err)) {
            return sh.fail("implementation-specific hook issues");
        }

        // All Good
        return true;
    }



    /**
    * @brief Deserialize this object for import following a previous `serialize()`.
    *
    * Deserialization happens in the same order as serialization, obviously.
    *
    * @note This method does not throw.
    * @param in The stream to read data from.
    * @param err Pointer to a string where errors, if any, are written.
    * @return A boolean indicating success or failure.  Do not discard.
    */
    [[nodiscard]] bool deserialize(std::istream& in, std::string* err = nullptr) {
        StreamHelper sh(&in, nullptr, err);
        sh.set_category("BinaryTree");
        EqualityHelper eq(err);

        // Reset this object.
        _impl.reset();

        // Header
        const BinaryTreeFileHeader header;
        char magic[header.MAGIC_SIZE];
        if (! sh.read_bytes(&magic, header.MAGIC_SIZE)) {
            return sh.fail("couldn't read header magic string");
        }
        if (! eq.equal(std::string(magic, header.MAGIC_SIZE), std::string(header.magic, header.MAGIC_SIZE))) {
            return sh.fail("Magic in header doesn't match the known value.  Wrong file?");
        }
        uint32_t version;
        if (! sh.deserialize_integral(version)) {
            return sh.fail("couldn't read header version");
        }
        if (! eq.equal(version, header.version)) {
            return sh.fail("Version mismatch");
        }

        // Common Metadata
        uint8_t tree_type;
        uint32_t level_count;
        bool b_is_preserving_ancestors;
        bool b_is_verifying_non_hwm_nodes;
        // Tree type.  Has to match.
        if (! sh.deserialize_integral(tree_type)) {
            return sh.fail("couldn't read tree_type");
        }
        if (! eq.equal(tree_type, static_cast<uint8_t>(_impl.get_tree_type()))) {
            return sh.fail("Tree types don't match");
        }
        // Level count.  Type T needs to support this many levels.
        if (! sh.deserialize_integral(level_count)) {
            return sh.fail("couldn't read level_count");
        }
        if (! BinaryTreeMath<T>::st_level_will_fit(level_count)) {
            std::string msg = "Level count found ("
                + to_string_any(level_count)
                + ") exceeds the amount permitted ("
                + to_string_any(BinaryTreeMath<T>::st_max_level_of_type())
                + ") by the type T you specified: "
                + ABIHelpers::demangle(typeid(T).name())
                + ".";
            return sh.fail(msg);
        }
        _impl.set_level_count(level_count);
        // Is preserving ancestors
        if (! sh.deserialize_bool(b_is_preserving_ancestors)) {
            return sh.fail("couldn't read is_preserving_ancestors");
        }
        _impl.set_is_preserving_ancestors(b_is_preserving_ancestors);
        // Is verifying non-hwm nodes
        if (! sh.deserialize_bool(b_is_verifying_non_hwm_nodes)) {
            return sh.fail("couldn't read is_verifying_non_hwm_nodes");
        }
        if (b_is_verifying_non_hwm_nodes) {
            enable_non_hwm_node_verification();
        } else {
            disable_non_hwm_node_verification();
        }
        // The is_initialized must be true, so we set it manually.
        _impl.set_is_initialized(true);

        // Root node
        bool source_has_root_node;
        if (! sh.deserialize_bool(source_has_root_node)) {
            return sh.fail("has_root_node");
        }
        if (source_has_root_node) {
            Node<T>*& root_node = _impl.get_root_node_rw();
            // Root node can be null on *this* object, so make one for it to use.
            if (root_node == nullptr) {
                root_node = new Node<T>(BinaryTreeMath<T>::get_root_value());
            }
            T root_parent_v;
            T root_hwm_ancestor_v;
            uint8_t child_count;
            if (! root_node->deserialize(in, root_parent_v, root_hwm_ancestor_v, child_count, err)) {
                return sh.fail("root node");
            }
            if (root_parent_v != 0) {
                return sh.fail("Root node's parent value must be 0 (absent), but it's: " + to_string_any(root_parent_v));
            }
            if (root_hwm_ancestor_v != 0) {
                return sh.fail("Root node's hwm ancestor value must be 0 (absent), but it's: " + to_string_any(root_hwm_ancestor_v));
            }
        } else {
            // The backed up file didn't have a root node... which means it was a 0-level tree :/
            if (_impl.get_root_node() != nullptr) {
                Node<T>*& root_node = _impl.get_root_node_rw();
                delete root_node;
                root_node = nullptr;
            }
        }

        // Coverage
        std::unordered_map<level_t, BinaryTreeCoverage<T>>& coverage_map = _impl.get_coverage_map_rw();
        uint64_t u64_entry_count = 0;
        if (! sh.deserialize_integral(u64_entry_count)) {
            return sh.fail("couldn't get coverage's entry count");
        }
        for (uint64_t i = 0; i < u64_entry_count; i++) {
            uint64_t u64_level;
            if (! sh.deserialize_integral(u64_level)) {
                return sh.fail("couldn't read coverage level for index i==" + to_string_any(i));
            }
            level_t level = static_cast<level_t>(u64_level);
            BinaryTreeCoverage<T> coverage;
            if (! coverage.deserialize(in, err)) {
                return sh.fail("coverage on level " + to_string_any(level));
            }
            // Level and coverage are okay.  Throw them in the map.
            coverage_map[level] = coverage;
        }

        // Ancestors
        // These are new-placement (no children).  Slam them into the vector.
        uint64_t u64_size = 0;
        std::vector<Node<T>*>& ancestors_rw = _impl.get_ancestors_rw();
        if (! sh.deserialize_integral(u64_size)) {
            return sh.fail("couldn't read ancestors.size() count");
        }
        ancestors_rw.reserve(static_cast<size_t>(u64_size));
        T parent_v;
        T hwm_ancestor_v;
        uint8_t child_count;
        for (uint64_t i = 0; i < u64_size; i++) {
            Node<T>* new_node = new Node<T>(T(1));
            if (! new_node->deserialize(in, parent_v, hwm_ancestor_v, child_count, err)) {
                return sh.fail("couldn't deserialize ancestor at index i==" + to_string_any(i));
            }
            ancestors_rw.push_back(new_node);
        }

        // Implementation-specific hook
        if (! _impl.deserialize(in, err)) {
            return sh.fail("implementation-specific hook issues");
        }

        // All good
        return true;
    }



    /**
    * @brief Writes data from `serialize()` to the path specified.
    * @param path The string path to write to.  When path is "-", will write to stdout.  Throws error if stdout is a terminal.
    * @param compression_level The Zstd compression level to apply.  Options are:
    *          * 0) No compression.  A raw file.
    *          * 1-19) Zstd compression with normal range and end-directives (see Zstd internal.h).
    *          * 20-22) Zstd compression with long distance matching enabled and end-directive set to "flush" for better threading.
    * @note All compressed files are true Zstd files (header, frames, etc), and are therefore fully compatible with other Zstd
    *       tools, such as `zstd` CLI.  If the compression you seek isn't possible with this method's settings, you may write to
    *       stdout and pipe to the `zstd` CLI with any settings you desire.  The associated `load()` method can ready any valid
    *       Zstd file.
    * @return True if successful, false otherwise.
    */
    bool save(const std::string& path, int compression_level = 22) {
        // Sanity checks.
        if (compression_level > 22) {
            throw std::out_of_range("Zstd compression cannot exceed 22.  You sent " + to_string_any(compression_level) + " to BinaryTree.save()");
        }
        if (compression_level < 0) {
            throw std::out_of_range("While Zstd supports negative compression levels, we do not.  You sent " + to_string_any(compression_level) + " to BinaryTree.save()");
        }

        // Setup some pointers to the file output and a potential zstd out.
        std::ofstream file_out;
        std::ostream* final_out = nullptr;
        std::unique_ptr<zstd_ostream> zstd_out;

        // Open the file stream.
        const bool use_compression = compression_level > 0;
        if (path == "-") {
            // Write to stdout.  Ensure it's not a terminal.
            if (isatty(fileno(stdout))) {
                throw std::runtime_error("Refusing to write binary data to a terminal.  Redirect to a file, pipe, etc.");
            }
            final_out = &std::cout;
        } else {
            // Write to a normal file.
            file_out.open(path, std::ios::binary);
            if (!file_out) {
                throw std::runtime_error("Failed to open file for writing");
            }
            final_out = &file_out;
        }

        // If compression is enabled, thread it into our zstd_ostream helper.
        if (use_compression) {
            zstd_out = std::make_unique<zstd_ostream>(*final_out, compression_level);
            final_out = zstd_out.get();
        }

        // Perform the serialization and see if it worked.
        std::string err;
        bool ok = serialize(*final_out, &err);

        // If compression is used, zstd requires finialization logic to finish a frame.
        if (use_compression) {
            zstd_out->finalize();
        } else {
            // We'll flush a raw file here just for good practice.
            final_out->flush();
        }
        if (! ok) {
            throw std::runtime_error("Failed to serialize data.  Error chain is:  " + err);
        }

        return ok && final_out->good();
    }



    /**
    * @brief Read `deserialize()` data from the path specified.  Data may be raw or compressed with Zstd.
    * @param path The path to the file to read.  When path is "-", will read from stdin.
    * @return True if successful, false otherwise.
    */
    bool load(const std::string& path) {
        // Setup pointers to the file input and a potential zstd in.
        std::ifstream file_in;
        std::istream* final_in = nullptr;
        std::unique_ptr<zstd_istream> zstd_in;

        // Open the file stream.
        if (path == "-") {
            // Read from stdin.
            final_in = &std::cin;
        } else {
            // Read from a normal file.
            file_in.open(path, std::ios::binary);
            if (!file_in) {
                throw std::runtime_error("Failed to open file for reading");
            }
            final_in = &file_in;
        }

        // Check for Zstd compression.  Then rewind our cursor.
        std::string err;
        uint32_t four_byte_magic;
        {
            StreamHelper sh(final_in, nullptr, &err);
            std::streampos pos = final_in->tellg();
            if (! sh.deserialize_integral(four_byte_magic)) {
                throw std::runtime_error("Failed to deserialize first four bytes to detect Zstd compression.  Error chain is: " + err);
            }
            final_in->clear();
            final_in->seekg(pos);
        }

        // If we have Zstd compression, we need to link the file to our decompressor and pass on a different istream.
        const bool is_zstd = four_byte_magic == ZSTD_MAGICNUMBER;
        if (is_zstd) {
            zstd_in = std::make_unique<zstd_istream>(*final_in);
            final_in = zstd_in.get();
        }

        // Now we can deserialize and it'll only receive the raw data.
        err.clear();
        if (! deserialize(*final_in, &err)) {
            throw std::runtime_error("Failed to deserialize data (zstd compressed=" + to_string_any(is_zstd) + ").  Error chain is:  " + err);
        }

        return true;
    }



    /**
    * @brief Generate a `Node` at the level and position specified.  It will not be part of any tree.
    * @param level The level of the node.
    * @param position The left-to-right position of the node.
    * @return Pointer to a `Node` object, not associated with any tree.
    */
    static Node<T>* st_generate_node_at(level_t level, T position) {
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



/// @brief Shorthand for a `BinaryTree<T, BinaryTreeMaterializedImpl<T>>`.
/// @tparam T Any supported integral (see concepts.hpp).
template<AnySupportedIntegral T>
using MaterializedBinaryTree = BinaryTree<T, BinaryTreeMaterializedImpl<T>>;
/// @brief Shorthand for a `BinaryTree<T, BinaryTreeImplicitImpl<T>>`.
/// @tparam T Any supported integral (see concepts.hpp).
template<AnySupportedIntegral T>
using ImplicitBinaryTree = BinaryTree<T, BinaryTreeImplicitImpl<T>>;
