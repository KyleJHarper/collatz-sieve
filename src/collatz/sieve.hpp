#pragma once
#include "concepts.hpp"
#include "binary_tree.hpp"
#include "gmp_helpers.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>
#include "node.hpp"
#include <execution>





//
// Options Package for Sieve
//
struct SieveOptions {
    size_t pool_size = 4096;
    BinaryTreeOptions tree_opts = {
        .track_node_metadata = false,
        .prune_hwm_nodes = true,
        .prune_parent_levels = true,
        .preserve_ancestors = true,
    };
};





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
    std::vector<uint64_t> _survivors;
    uint64_t _survivor_index;
    uint64_t _survivor_count;
    uint64_t _step;
    uint64_t _multiplier;
    uint64_t _incrementer;
    //
    // Keep a cache of future values to avoid allocation logic in multiple methods.
    std::vector<T> _pool;
    size_t _pool_size;
    size_t _pool_index;



    public:
    static constexpr SieveOptions DEFAULT_OPTS{};

    //
    // Constructors
    //
    // Default Constructor.
    Sieve() {}
    //
    // User-supplied tree.
    Sieve(BinaryTree<T>& tree, SieveOptions opts = Sieve<T>::DEFAULT_OPTS) {
        init(&tree, opts);
    }
    //
    // Sieve-built tree.
    Sieve(size_t tree_levels, SieveOptions opts = Sieve<T>::DEFAULT_OPTS) {
        BinaryTree<T>* tree = new BinaryTree<T>(tree_levels, opts.tree_opts);
        init(tree, opts);
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
    void init(BinaryTree<T>* tree, SieveOptions opts = Sieve<T>::DEFAULT_OPTS) {
        // Reset the multiplier and step values.
        _multiplier = 1;
        _step = BinaryTreeMath<uint64_t>::st_step(tree->get_level_count());

        // Build the incrementer based on step and multiplier.
        _incrementer = (_multiplier * _step);

        // Get a reference to the last level.
        const std::vector<Node<T>*>& last_level = tree->get_level_map().at(tree->get_level_count());

        // Resize the survivor list to keep memory close.
        _survivor_index = 0;
        _survivor_count = last_level.size();
        _survivors.clear();
        _survivors.shrink_to_fit();
        _survivors.resize(_survivor_count);

        // Resize and clear the cache.
        _pool_size = opts.pool_size;
        _pool.clear();
        _pool.shrink_to_fit();
        _pool.resize(_pool_size);

        // Build a list of surviving children.
        #pragma omp parallel for schedule(static, 1000) default(none) shared(_survivors, last_level)
        for (uint64_t i = 0; i < _survivor_count; i++) {
            Node<T>* survivor = last_level[i];
            if constexpr(std::integral<T>) {
                _survivors[i] = survivor->get_value();
            } else {
                // Convert the GMP to uint64_t.  See private member for why.
                _survivors[i] = mpz_get_ui64(survivor->get_value().get_mpz_t());
            }
        }

        // Now sort the T list.  This can be millions of items.
        std::sort(std::execution::par, _survivors.begin(), _survivors.end());

        // Refill the pool.  Extend the pool index to ensure a full flush.
        _pool_index = _pool_size;
        refill_pool();
    }



    //
    // Getters
    //
    const std::vector<uint64_t>& get_survivors() const { return _survivors; }
    uint64_t get_step() { return _step; }
    uint64_t get_multiplier() { return _multiplier; }
    uint64_t get_incrementer() { return _incrementer; }
    const std::vector<T>& get_pool() const { return _pool; }
    size_t get_pool_index() const { return _pool_index; }
    size_t get_pool_size() const { return _pool_size; }



    //
    // Refill Pool
    //
    void refill_pool() {
        // Move unused pool objects to the front if refill was called early.
        size_t fill_index = 0;
        if (_pool_index < _pool_size) {
            #pragma omp parallel for default(none) schedule(static, 1000) shared(_pool_index)
            for (size_t i = _pool_index; i < _pool_size; i++) {
                _pool[i - _pool_index] = _pool[i];
            }

            // Change the fill index to end after our last move.
            fill_index = _pool_index;
        }

        // Reset the pool index safely now.
        _pool_index = 0;

        // Now fill.
        for (size_t i = fill_index; i < _pool_size; i++) {
            _pool[i] = _survivors[_survivor_index] + _incrementer;
            bump_survivor_index();
        }
    }



    //
    // Increment Offset
    // Bump the offset and perform overflow.
    //
    inline void bump_survivor_index() {
        _survivor_index += 1;
        if (_survivor_index >= _survivor_count) {
            _survivor_index = 0;
            _multiplier += 1;
            _incrementer = _step * _multiplier;
        }
    }



    //
    // Next
    // Returns the next value from the sieve.  This is a single-threaded, one-by-one iterator.
    //
    T next() {
        T result = _pool[_pool_index++];
        if (_pool_index >= _pool_size) {
            refill_pool();
        }
        return result;
    }
    //
    // Overload to get multiple.  Fills your buffer from 0 to buffer.capacity().  Threaded.
    void next(std::vector<T>& buffer) {
        size_t filled = 0;
        while (filled < buffer.capacity()) {
            size_t cache_left = _pool.capacity() - _pool_index;
            size_t limit = std::min(buffer.capacity() - filled, cache_left);
            for (size_t i = 0; i < limit; i++) {
                buffer[filled + i] = _pool[_pool_index + i];
            }
            filled += limit;
            _pool_index += limit;
            if (_pool_index >= _pool_size) {
                refill_pool();
            }
        }
        std::cout << "We should support arrays too.  Templated?\n";
    }
};
