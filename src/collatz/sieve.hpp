#pragma once
#include "concepts.hpp"
#include "binary_tree.hpp"
#include <vector>
#include "node.hpp"
#include <tbb/tbb.h>
#include <execution>




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
    //
    // Big Fat Note About Data Types
    //
    // We can't fit more than 2^64 nodes in memory.  Even at 1 byte that would be 16 exabytes.  Even if we reduced the test space
    // by 99.9%, a 64-level tree would have ~18 quadrillion survivors and 16 petabytes of RAM.  This means neither the count, step,
    // offset, or even survivor list itself can exceed uint64_t.
    //
    // We can ISSUE values larger than 2^64 via .next(), so it'll be type T returned.  But none of the private members need it.
    //
    std::vector<size_t> _survivors;
    size_t _step = 0;
    size_t _offset = 0;
    size_t _survivor_count = 0;
    size_t _multiplier = 1;
    size_t _incrementer;


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
        init(&tree);
    }
    //
    // Sieve-built tree.  Owns tree.
    Sieve(size_t tree_levels, BinaryTreeOptions opts = Sieve<T>::SIEVE_PRUNE_DEFAULTS) {
        BinaryTree<T>* tree = new BinaryTree<T>(tree_levels, opts);
        init(tree);
    }



    //
    // Destructor
    //
    ~Sieve() {
    }



    //
    // Initialize
    // Builds the object, reusing it if necessary.
    //
    void init(BinaryTree<T>* tree) {
        // Reset the multiplier and step values.
        _multiplier = 1;
        _step = BinaryTreeMath<T>::st_step(tree->get_level_count());

        // Build the incrementer based on step and multiplier.
        _incrementer = (_multiplier * _step);

        // Get a reference to the last level.
        std::vector<Node<T>*>& last_level = tree->get_level_map(tree->get_level_count());

        // Resize the survivor list to keep memory close.
        _survivors.clear();
        _survivors.shrink_to_fit();
        _survivors.reserve(last_level.size());

        // Build a list of surviving children.
        for (Node<T>* survivor : tree->get_level_map()[tree->get_level_count()]) {
            if constexpr(std::integral<T>) {
                _survivors.push_back(survivor->get_value());
            } else {
                // Move the GMP object.  Don't alloc.
                _survivors.push_back(std::move(survivor->get_value()));
            }
        }

        // Save the count externally.  Probably unecessary, but whatever.
        _survivor_count = _survivors.size();

        // Now sort the T list.  This can be millions of items.
        std::sort(std::execution::par, _survivors.begin(), _survivors.end());
    }



    //
    // Increment Offset
    // Bump the offset and perform overflow.
    //
    inline void bump_offset() {
        _offset += 1;
        if (_offset >= _survivor_count) {
            _offset = 0;
            _multiplier += 1;
            _incrementer = _step * _multiplier;
        }
    }



    //
    // Next
    // Returns the next value from the sieve.  This is a single-threaded, one-by-one iterator.
    //
    T next() {
        T result = _survivors[_offset];
        if constexpr(std::integral<T>) {
            result += _incrementer;
        } else {
            mpz_add_ui(result.get_mpz_t(), result.get_mpz_t(), _incrementer);
        }
        bump_offset();
        return result;
    }

};
