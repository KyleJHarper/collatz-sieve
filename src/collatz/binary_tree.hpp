#pragma once

#include "binary_tree_materialized.hpp"
#include "binary_tree_implicit.hpp"
#include "binary_tree_types.hpp"
#include "concepts.hpp"
#include <stdexcept>
#include <type_traits>
#include "stream_helpers.hpp"
#include <fstream>
#include "equality_helper.hpp"
#include <tbb/parallel_sort.h>
#include "abi_helpers.hpp"




//
// Concepts for Requires
//
template<typename TreeType>
concept IsMaterializedTree = requires(const TreeType& tree_type) { tree_type.is_materialized(); };
template<typename TreeType>
concept IsImplicitTree = requires(const TreeType& tree_type) { tree_type.is_implicit(); };




//
// Structures and such for exporting/saving/loading.
//
struct BinaryTreeFileHeader {
    static const uint32_t MAGIC_SIZE = 10;
    const char magic[MAGIC_SIZE] = {'H', 'a', 'r', 'p', 'e', 'r', 'T', 'r', 'e', 'e'}; // "HarperTree"
    const uint32_t version = 1;
};




//
// The BinaryTree facade.  User must specify a TreeType for implmentation.
//
// template<AnySupportedIntegral T, typename TreeType = BinaryTreeImplicitImpl<T>>
template<AnySupportedIntegral T, typename TreeType = BinaryTreeImplicitImpl<T>>
class BinaryTree {
    private:
    TreeType _impl;



    public:
    //
    // Constructors
    //
    using backend_type = TreeType;
    static constexpr BinaryTreeOptions DEFAULT_OPTS{};
    BinaryTree() = default;
    BinaryTree(size_t levels, const BinaryTreeOptions& opts = DEFAULT_OPTS) {
        init(levels, opts);
    }



    //
    // Destructor
    //
    ~BinaryTree() = default;



    //
    // Getters and Accessors
    //
    const TreeType& get_impl() const { return _impl; }
    TreeTypeEnum get_tree_type() const { return _impl.get_tree_type(); }
    size_t get_level_count() const { return _impl.get_level_count(); }
    Node<T>* get_root_node() const { return _impl.get_root_node(); }
    const std::unordered_map<size_t, BinaryTreeCoverage<T>>& get_coverage_map() const { return _impl.get_coverage_map(); }
    const std::vector<Node<T>*>& get_ancestors() const { return _impl.get_ancestors(); }
    bool is_preserving_ancestors() const { return _impl.is_preserving_ancestors(); }
    bool is_verifying_non_hwm_nodes() const { return _impl.is_verifying_non_hwm_nodes(); }
    void disable_non_hwm_node_verification() { _impl.disable_non_hwm_node_verification(); }
    void enable_non_hwm_node_verification() { _impl.enable_non_hwm_node_verification(); }
    bool is_initialized() const { return _impl.is_initialized(); }
    T node_count() const { return BinaryTreeMath<T>::st_node_count_of_tree(_impl.get_level_count()); }
    //
    // Materialized-Specific
    bool is_materialized() const requires IsMaterializedTree<TreeType> { return true; }
    const std::unordered_map<size_t, std::vector<Node<T>*>>& get_level_map() const requires IsMaterializedTree<TreeType> {
        return _impl.get_level_map();
    }
    bool is_pruning_hwm_nodes() const requires IsMaterializedTree<TreeType> {
        return _impl.is_pruning_hwm_nodes();
    }
    bool is_pruning_parent_levels() const requires IsMaterializedTree<TreeType> {
        return _impl.is_pruning_parent_levels();
    }
    T real_node_count() const requires IsMaterializedTree<TreeType> {
        return _impl.real_node_count();
    }
    //
    // Implicit-Specific
    bool is_implicit() const requires IsImplicitTree<TreeType> { return true; }
    const NodeBitmap<T>& get_uncovered_positions() const requires IsImplicitTree<TreeType> {
        return _impl.get_uncovered_positions();
    }



    //
    // Assert Level Fits
    // Helper to determine if the level requested is going to fit within the bit-size of T.
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
    // build an API that allows people to build trees beyond empirically tested boundaries unless they are willing to verify new
    // nodes themselves.
    //
    void assert_level_verification(size_t level, bool is_verifying_non_hwm_nodes) {
        if (is_verifying_non_hwm_nodes == false && level >= CollatzConstants::LARGEST_EPIRICALLY_TESTED_LEVEL + 1) {
            std::string msg= "Tree has reached max level of previously verified space: "
            + to_string_any(level)
            + ".  You have disabled runtime verification, which means this and future levels are NOT fully verified!"
            + "  Refusing to build level.";
            throw std::runtime_error(msg);
        }
    }



    //
    // Initialize
    // Builds the object, reusing it if necessary.
    //
    void init(size_t levels, const BinaryTreeOptions& opts = DEFAULT_OPTS) {
        // Sanity check.  T must support requested tree size.
        assert_level_will_fit(levels);
        _impl.init(levels, opts);
    }



    //
    // Reset Object
    // Reset members to make this act like a new() object.
    //
    void reset() { _impl.reset(); }



    //
    // Add Level
    // Add a level to the tree.  Details are implementation-specific.
    //
    void add_level() {
        size_t next_level = _impl.get_level_count() + 1;
        assert_level_will_fit(next_level);
        assert_level_verification(next_level, _impl.is_verifying_non_hwm_nodes());
        _impl.add_level();
    }



    //
    // Deep Size
    //
    size_t deep_size() const { return _impl.deep_size(); }



    //
    // Equal
    //
    // Compare all of the members and (meta) data.  Implementation specific details are handed off.
    //
    // Returns true if they are equal in representation.  False otherwise.
    // Will explain what failed to *err if sent.
    //
    static bool st_equal(const BinaryTree<T, TreeType>& first, const BinaryTree<T, TreeType>& second, std::string* err = nullptr) {
        // Common to all trees.
        EqualityHelper eq(err);

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
    //
    // Member helper.
    bool equal(const BinaryTree<T, TreeType>& second, std::string* err = nullptr) const {
        return BinaryTree<T, TreeType>::st_equal(*this, second, err);
    }



    //
    // Serialize
    // Serialize data into an output.
    //
    // The general file structure is:
    //
    // [HEADER]
    // [COMMON METADATA]
    // [ROOT NODE]
    // [COVERAGE]
    // [ANCESTORS]
    // [MATERIAL OR IMPLICIT SPECIFICS]
    //
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
        if (! get_root_node()->serialize(out, err)) {
            return sh.fail("root node");
        }

        // Coverage
        // Write out how many entries exist so we know how many loops to make.
        const std::unordered_map<size_t, BinaryTreeCoverage<T>>& coverage_map = _impl.get_coverage_map();
        uint64_t u64_entry_count = coverage_map.size();
        if (! sh.serialize_integral(u64_entry_count)) {
            return sh.fail("coverage entry_count==" + to_string_any(u64_entry_count));
        }
        // Loop through them, emitting the level and then the coverage itself.
        // Sort the levels to ensure consistent output.  Nodes themselves are always ordered consistently.
        std::vector<size_t> coverage_levels;
        coverage_levels.reserve(coverage_map.size());
        for (const auto& [level, _] : coverage_map) {
            coverage_levels.push_back(level);
        }
        std::sort(coverage_levels.begin(), coverage_levels.end());
        for (size_t level : coverage_levels) {
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



    //
    // Deserialize
    // Read previously-serialize()'d data from a stream and reconstruct an object of this type.
    //
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
        Node<T>* root_node = _impl.get_root_node_rw();
        T root_parent_v;
        T root_hwm_ancestor_v;
        if (! root_node->deserialize(in, root_parent_v, root_hwm_ancestor_v, err)) {
            return sh.fail("root node");
        }
        if (root_parent_v != 0) {
            return sh.fail("Root node's parent value must be 0 (absent), but it's: " + to_string_any(root_parent_v));
        }
        if (root_hwm_ancestor_v != 0) {
            return sh.fail("Root node's hwm ancestor value must be 0 (absent), but it's: " + to_string_any(root_hwm_ancestor_v));
        }

        // Coverage
        std::unordered_map<size_t, BinaryTreeCoverage<T>>& coverage_map = _impl.get_coverage_map_rw();
        uint64_t u64_entry_count = 0;
        if (! sh.deserialize_integral(u64_entry_count)) {
            return sh.fail("couldn't get coverage's entry count");
        }
        for (uint64_t i = 0; i < u64_entry_count; i++) {
            uint64_t u64_level;
            if (! sh.deserialize_integral(u64_level)) {
                return sh.fail("couldn't read coverage level for index i==" + to_string_any(i));
            }
            size_t level = static_cast<size_t>(u64_level);
            BinaryTreeCoverage<T> coverage;
            if (! coverage.deserialize(in, err)) {
                return sh.fail("coverage on level " + to_string_any(level));
            }
            // Level and coverage are okay.  Throw them in the map.
            coverage_map[level] = coverage;
        }

        // Ancestors
        // These are new-placement (no children).  Only their value was emitted.  Slam them into the vector.
        uint64_t u64_size = 0;
        std::vector<Node<T>*>& ancestors = _impl.get_ancestors_rw();
        if (! sh.deserialize_integral(u64_size)) {
            return sh.fail("couldn't read ancestors.size() count");
        }
        ancestors.reserve(static_cast<size_t>(u64_size));
        T new_node_value;
        for (uint64_t i = 0; i < u64_size; i++) {
            if (! sh.deserialize_integral(new_node_value)) {
                return sh.fail("couldn't read node value on index i==" + to_string_any(i));
            }
            Node<T>* new_node = new Node<T>(new_node_value);
            ancestors.push_back(new_node);
        }

        // Implementation-specific hook
        if (! _impl.deserialize(in, err)) {
            return sh.fail("implementation-specific hook issues");
        }

        // All good
        return true;
    }



    //
    // Save
    // Writes the serialize() data to the path specified.  Returns true if successful.
    //
    bool save(const std::string& path) {
        std::ofstream f_out(path, std::ios::binary);
        if (!f_out) {
            throw std::runtime_error("Failed to open file for writing");
        }

        std::string err;
        if (! serialize(f_out, &err)) {
            throw std::runtime_error("Failed to serialize data.  Error chain is:  " + err);
        }
        f_out.flush();

        return f_out.good();
    }



    //
    // Load
    // Reads the deserialize() data from the path specified.  Returns true if successful.
    //
    bool load(const std::string& path) {
        std::ifstream f_in(path, std::ios::binary);
        if (!f_in) {
            throw std::runtime_error("Failed to open file for reading");
        }

        std::string err;
        if (! deserialize(f_in, &err)) {
            throw std::runtime_error("Failed to deserialize data.  Error chain is:  " + err);
        }

        return f_in.good();
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
// Aliases
// Two quick aliases to improve readability.
//
template<AnySupportedIntegral T>
using MaterializedBinaryTree = BinaryTree<T, BinaryTreeMaterializedImpl<T>>;
template<AnySupportedIntegral T>
using ImplicitBinaryTree = BinaryTree<T, BinaryTreeImplicitImpl<T>>;
