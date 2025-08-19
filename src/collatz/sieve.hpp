#pragma once
#include "concepts.hpp"
#include "binary_tree.hpp"




//
// Sieve
//
// An implementation of my algorithm to reject (filter out) values when space testing for Collatz sequences.
//
// The algorithm is baked into the BinaryTree and BinaryTreeMath, instrinic to how it's built.  High-water mark (HWM) nodes are
// flagged and removed, along with all descendants.  This leaves only the nodes who haven't hit HWM.
//
// Further optimizations, such as a forward-looking cache are still TBD.
//
template<IntegralOrMPZClass T>
class Sieve {
    private:
    BinaryTree<T>* _tree = nullptr;
    bool _owns_tree = false;
    size_t _current_level = 0;
    T _current_position = 1;
    T _max_position = 1;


    public:
    // Binary Tree Options should default to pruned.
    static constexpr BinaryTreeOptions SIEVE_PRUNE_DEFAULTS = {
        .track_node_metadata = false,
        .prune_hwm_nodes = true,
        .prune_parent_levels = true,
        .preserve_ancestors = true,
    };



    //
    // Constructors
    //
    // Default Constructor.
    Sieve() {}
    //
    // User-defined tree.  Store a reference.  Do not own.
    Sieve(BinaryTree<T>& tree) {
        init(&tree, false);
    }
    //
    // Sieve-built tree.  Owns tree.
    Sieve(size_t tree_levels, BinaryTreeOptions opts = Sieve<T>::SIEVE_PRUNE_DEFAULTS) {
        BinaryTree<T>* tree = new BinaryTree<T>(tree_levels, opts);
        init(tree, true);
    }



    //
    // Destructor
    //
    ~Sieve() {
        if (_owns_tree) {
            delete _tree;
        }
    }



    //
    // Initialize
    // Builds the object, reusing it if necessary.
    //
    void init(BinaryTree<T>* tree, bool owns_tree) {
        _tree = tree;
        _owns_tree = owns_tree;
        _current_level = _tree->get_level_count() + 1;
        _current_position = 1;
        _max_position = tree->get_level_map()[tree->get_level_count()].size();
    }



    //
    // Next
    // Returns the next value from the sieve.  This is a single-threaded, one-by-one iterator.
    //
    T next() {
        // What if we just treat each parent node as the root of a subtree, which will be a perfect binary tree unto itself.
        // We then iterate through it, moving to the next parent and calling get_descendants() or whatever as we go.
        //
        // We'll need the same protection against memory explosion as we have when building the tree.  We'll need a yielding
        // function and/or tracking of the current descendant position so we can have reentrance.



        // Bump position.
        // How do we handle left/right?
        // The level and position are for the next node.  We need to find the parent.
        _current_position += 1;
        if (_current_position > _max_position) {
            _current_level += 1;
            _current_position = 1;
        }

        // Who is the parent?
        // If the position is odd/even that tells us the extra offset, right?
        // We have to do that at each level, right?
        // Pretty sure we need to offload this to BinaryTreeMath...

        // Maybe it's easier to track parent_position and offset according to level delta?
        // L4       parent
        //         /     \
        // L5    c1       c2
        //      / \       / \
        // L6  g1  g2    g1  g2
        //
        // position = 1
        // max_descendents = 2^(L6 - L4) == 2^2 == 4
        // val(g1) == ??
        // val(g2) == ??
        //

    }

};
