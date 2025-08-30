#pragma once
#include "concepts.hpp"
#include "binary_tree.hpp"
#include "gmp_helpers.hpp"
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <limits>
#include <ratio>
#include <stdexcept>
#include <string>
#include <vector>
#include "node.hpp"
#include <execution>
#include <chrono>





//
// Options Package for Sieve
//
struct SieveOptions {
    size_t pool_size = size_t{1} << 20;  // 1,048,576  (~1M)
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
    // offset, or even survivor list itself can exceed uint64_t.  We will use size_t for counts/indexes to maintain compatibility
    // with C++/STL types, like std::vector and its .size().
    //
    // We can ISSUE values larger than 2^64 via .next(), so it'll be type T returned.  But none of the private members need it.
    //
    std::vector<uint64_t> _survivors;
    size_t _survivor_index;
    size_t _survivor_count;
    uint64_t _step;
    uint64_t _multiplier;
    uint64_t _max_multiplier;
    //
    // Incrementer can grow beyond 64 bit.  E.g.: a 62-level tree with step 2^62 and a multiplier > 4 would break it.
    T _incrementer;
    //
    // Keep a cache of future values to avoid allocation logic in multiple methods.
    std::vector<T> _pool;
    size_t _pool_size;
    size_t _pool_index;
    size_t _pool_generated_count;
    size_t _pool_refills;
    size_t _pool_premature_refills;
    size_t _pool_refill_fill_loops;
    std::chrono::duration<double, std::micro> _pool_refill_time_us;
    //
    // Tree metadata.
    size_t _tree_level_count;



    public:
    static constexpr SieveOptions DEFAULT_OPTS{};

    //
    // Constructors
    //
    // Default Constructor.
    Sieve() = delete;
    //
    // User-supplied tree.
    Sieve(BinaryTree<T>& tree, SieveOptions opts = Sieve<T>::DEFAULT_OPTS) {
        init(&tree, opts);
    }
    //
    // Sieve-built tree.
    Sieve(size_t tree_levels, SieveOptions opts = Sieve<T>::DEFAULT_OPTS) {
        BinaryTree<T> tree(tree_levels, opts.tree_opts);
        init(&tree, opts);
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
        // Get a reference to the last level.
        const std::vector<Node<T>*>& last_level = tree->get_level_map().at(tree->get_level_count());

        // Resize the survivor list to keep memory close.
        _survivor_index = 0;
        _survivor_count = last_level.size();
        _survivors.clear();
        _survivors.shrink_to_fit();
        _survivors.resize(_survivor_count);

        // Resize and clear the cache.
        if (opts.pool_size == 0) {
            throw std::invalid_argument("pool_size must be > 0");
        }
        _pool_size = opts.pool_size;
        _pool.clear();
        _pool.shrink_to_fit();
        _pool.resize(_pool_size);
        _pool_generated_count = 0;
        _pool_refills = 0;
        _pool_premature_refills = 0;
        _pool_refill_fill_loops = 0;

        // Build a list of surviving children.
        // #pragma omp parallel for schedule(static) default(none) shared(_survivors, last_level)
        for (size_t i = 0; i < _survivor_count; i++) {
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

        // Reset the multiplier and step values.  Recalculate incrementer.
        _multiplier = 1;
        _step = BinaryTreeMath<uint64_t>::st_step(tree->get_level_count());
        if constexpr(std::integral<T>) {
            _max_multiplier = std::floor((std::numeric_limits<uint64_t>::max() - _survivors.back()) / _step);
        } else {
            _max_multiplier = std::numeric_limits<uint64_t>::max();
        }
        recalculate_incrementer();


        // Refill the pool.  Extend the pool index to ensure a full flush.
        _pool_index = _pool_size;
        _pool_refill_time_us.zero();
        refill_pool();

        // Attach tree metadata before discarding tree.
        _tree_level_count = tree->get_level_count();
    }



    //
    // Getters
    //
    const std::vector<uint64_t>& get_survivors() const { return _survivors; }
    size_t get_survivor_count() const { return _survivor_count; }
    size_t get_survivor_index() const { return _survivor_index; }
    uint64_t get_step() const { return _step; }
    uint64_t get_multiplier() const { return _multiplier; }
    uint64_t get_max_multiplier() const { return _max_multiplier; }
    const T& get_incrementer() const { return _incrementer; }
    const std::vector<T>& get_pool() const { return _pool; }
    size_t get_pool_index() const { return _pool_index; }
    size_t get_pool_size() const { return _pool_size; }
    size_t get_pool_generated_count() const { return _pool_generated_count; }
    size_t get_pool_refills() const { return _pool_refills; }
    size_t get_pool_premature_refills() const { return _pool_premature_refills; }
    size_t get_pool_refill_fill_loops() const { return _pool_refill_fill_loops; }
    size_t get_tree_level_count() const { return _tree_level_count; }
    std::chrono::duration<double, std::micro> get_pool_refill_time_us() const { return _pool_refill_time_us; }



    //
    // Setters
    //
    void unsafe_coerce_step(uint64_t value) { _step = value; }
    void unsafe_coerce_multiplier(uint64_t value) { _multiplier = value; }



    //
    // Refill Pool
    // Optionally send a vector to fill first, then the pool will fill afterward.
    //
    void refill_pool(std::vector<T>* partial_buffer_ptr = nullptr, size_t* partial_buffer_fill_index_ptr = nullptr) {
        // Test for a valid partial buffer, if sent.
        if (partial_buffer_ptr != nullptr) {
            if (partial_buffer_fill_index_ptr == nullptr) {
                throw std::logic_error("You can't send a partial buffer without an index.");
            }
            if (partial_buffer_ptr->size() == 0) {
                throw std::logic_error("You can't send a partial buffer with a .size() of zero.  Resize() first.");
            }
        }
        // Use chrono a lot here.  Namespace it.  Start the clock.
        using namespace std::chrono;
        time_point start = high_resolution_clock::now();

        // Move unused pool objects to the front if refill was called early.
        // Do not parallelize this.  It creates an ABA situation reading+writing to _pool and itsn't worth it.
        size_t pool_fill_index = _pool_size - _pool_index;
        if (_pool_index > 0 && _pool_index < _pool_size) {
            _pool_premature_refills++;
            // #pragma omp parallel for default(none) schedule(static) shared(_pool, _pool_index)
            for (size_t i = 0; i < pool_fill_index; i++) {
                if constexpr(std::integral<T>) {
                    _pool[i] = _pool[i + _pool_index];
                } else {
                    std::swap(_pool[i + _pool_index], _pool[i]);
                }
            }
        }

        // Reset the pool index safely now.
        _pool_index = 0;

        // Now fill.  We'll try to fill the caller's partial buffer, if necessary, then the pool.
        std::vector<T>* current_buffer;
        size_t current_fill_index;
        if (partial_buffer_ptr != nullptr && partial_buffer_fill_index_ptr != nullptr) {
            current_buffer = partial_buffer_ptr;
            current_fill_index = *partial_buffer_fill_index_ptr;
        } else {
            current_buffer = &_pool;
            current_fill_index = pool_fill_index;
        }
        size_t fill_limit = 0;
        while (current_buffer != &_pool || current_fill_index < current_buffer->size()) {
            // Swap buffers if if we were filling a partial.
            if (current_buffer == partial_buffer_ptr && current_fill_index >= current_buffer->size()) {
                // Update the partial's count
                *partial_buffer_fill_index_ptr = current_fill_index;
                // Now swap to pool structures.
                current_buffer = &_pool;
                current_fill_index = pool_fill_index;
            }

            // We can only parallelize within the _surivior_index/count.  After it, _incrementor changes.
            // We also can't overflow the pool. Ergo, pick the lowest.
            fill_limit = std::min(_survivor_count - _survivor_index, current_buffer->size() - current_fill_index);

            // Loop within limits.  Skip it to avoid thread overhead if limit is 0 this round.
            if (fill_limit != 0) {
                #pragma omp parallel for default(none) schedule(static) shared(current_buffer, _survivors, _incrementer, _survivor_index, current_fill_index, fill_limit)
                for (size_t i = 0; i < fill_limit; i++) {
                    if constexpr(std::integral<T>) {
                        (*current_buffer)[current_fill_index + i] = _survivors[_survivor_index + i] + _incrementer;
                    } else {
                        mpz_add_ui((*current_buffer)[current_fill_index + i].get_mpz_t(), _incrementer.get_mpz_t(), _survivors[_survivor_index + i]);
                    }
                }
            }

            // Adjust counters.
            current_fill_index += fill_limit;
            _pool_generated_count += fill_limit;
            _pool_refill_fill_loops++;
            bump_survivor_index(fill_limit);
        }

        // Increment counter.
        time_point end = high_resolution_clock::now();
        _pool_refill_time_us += (end - start);
        _pool_refills++;
    }



    //
    // Incrementer
    // Handle incrementer so it respects type T.
    //
    inline void recalculate_incrementer() {
        // Build the incrementer based on step and multiplier.
        if constexpr(std::integral<T>) {
            _incrementer = _multiplier * _step;
        } else {
            _incrementer = _multiplier;
            mpz_mul_ui(_incrementer.get_mpz_t(), _incrementer.get_mpz_t(), _step);
        }
    }



    //
    // Increment Offset
    // Bump the offset and perform overflow detection and 64-bit limits.
    //
    inline void bump_survivor_index(size_t amount = 1) {
        _survivor_index += amount;
        if (_survivor_index > _survivor_count) {
            throw std::out_of_range("You can't bump survivor index beyond survivor count.");
        }
        if (_survivor_index == _survivor_count) {
            _survivor_index = 0;
            if constexpr(std::integral<T>) {
                if (_multiplier >= _max_multiplier) {
                    std::string msg = "Multiplier " + std::to_string(_multiplier);
                    msg += " exceeeds max allowed multiplier for 64-bit type (" + std::to_string(_max_multiplier) + ").";
                    msg += "  Pool values would overflow if we continued.";
                    throw std::overflow_error(msg);
                }
            } else {
                if (_multiplier >= _max_multiplier) {
                    std::string msg = "Multiplier " + std::to_string(_multiplier);
                    msg += " exceeeds max allowed multiplier for GMP type (" + std::to_string(_max_multiplier) + ").";
                    msg += "  Pool is okay, but multiplier itself would overflow.";
                    throw std::overflow_error(msg);
                }
            }
            _multiplier += 1;
            recalculate_incrementer();
        }
    }



    //
    // Next
    // Returns the next value from the sieve.  This is a single-threaded, one-by-one iterator.
    //
    void next(T& out) {
        if constexpr(std::integral<T>) {
            out = _pool[_pool_index++];
        } else {
            std::swap(_pool[_pool_index++], out);
        }
        if (_pool_index >= _pool_size) {
            refill_pool();
        }
    }
    //
    // Overload to get multiple.  Fills your buffer from 0 to buffer.size().  Threaded.
    void next(std::vector<T>& buffer) {
        if (buffer.size() == 0) {
            throw std::logic_error("You must resize() your vector buffer before we'll fill it.");
        }

        size_t filled = 0;
        while (filled < buffer.size()) {
            size_t pool_left = _pool_size - _pool_index;
            size_t limit = std::min(buffer.size() - filled, pool_left);
            #pragma omp parallel for default(none) schedule(static) shared(buffer, _pool, _pool_index, filled, limit)
            for (size_t i = 0; i < limit; i++) {
                if constexpr(std::integral<T>) {
                    buffer[filled + i] = _pool[_pool_index + i];
                } else {
                    std::swap(buffer[filled + i], _pool[_pool_index + i]);
                }
            }
            filled += limit;
            _pool_index += limit;
            if (_pool_index >= _pool_size) {
                refill_pool(&buffer, &filled);
                break;
            }
        }
    }



    //
    // Object Size
    // Deeply scan the object, including pool and buffers.
    //
    size_t deep_size() const {
        size_t total = sizeof(*this);

        // Account for _pool
        if constexpr(std::integral<T>) {
            total += (_pool.size() * sizeof(T));
        } else {
            total += (_pool.size() * gmp_deep_sizeof(_pool[0]));
        }

        // Forward Looking Cache
        // TODO

        // Done
        return total;
    }

};
