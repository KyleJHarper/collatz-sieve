#pragma once
#include "absl/container/flat_hash_map.h"
#include <array>
#include <cstdint>
#include <stdexcept>
#include "concepts.hpp"




//
// A forward-looking cache will hold values like any other cache, but its design is for forward values of N when processing Collatz
// sequences.  Since we test sequentially, this fits nicely.  An LRU handles eviction naturally.
//
// The FLCKey (key) is easy to generate because the value of N in binary format is it.  We leverage the Absl hashing internals for
// bucketing.  This is all we need: presence equals hit.  This makes it act like a set.  However, we use map because we perform
// bookkeeping for eviction too.
//
// LRU is our cache eviction strategy.  Not only does this keep bookkeeping overhead lower than other strategies, it naturally
// makes the eviction logic and load low: just pop the end.  Performance becomes:
//   Lookup: O(1)
//   Insert: O(1)
//   Delete: O(1)
//
//  Note: Performance values are approximate for amortized/average case.  Hashing/collisions can affect this, like all hash maps.
//
// This stuff is NOT thread-safe!  Use you own synchronization.
//



//
// LRU Tracker
// Tracks values in an LRU-style eviction.
//
template <typename Key>
class LRUTracker {
    private:
    struct Node {
        size_t prev = SIZE_MAX;
        size_t next = SIZE_MAX;
    };
    std::vector<Node> _nodes;
    std::vector<bool> _in_use;
    size_t _head = SIZE_MAX;
    size_t _tail = SIZE_MAX;
    size_t _capacity;



    public:
    //
    // Constructors
    //
    LRUTracker(size_t capacity) {
        init(capacity);
    }



    //
    // Initialize
    // Builds the object, reusing it if necessary.
    //
    void init(size_t capacity) {
        _capacity = capacity;
        _head = SIZE_MAX;
        _tail = SIZE_MAX;
        _nodes.clear();
        _nodes.shrink_to_fit();
        _nodes.resize(capacity);
        _in_use.clear();
        _in_use.shrink_to_fit();
        _in_use.resize(capacity, false);
    }



    //
    // Getters
    //
    size_t get_head() const { return _head; }
    size_t get_tail() const { return _tail; }
    const std::vector<Node>& get_nodes() const { return _nodes; }



    //
    // Insert
    // Add index to the front.  It if already exists, just touch it.
    //
    void insert(size_t idx) {
        if (_in_use[idx]) {
            touch(idx);
            return;
        }
        _in_use[idx] = true;
        push(idx);
    }



    //
    // Erase
    // Remove a node.
    //
    void erase(size_t idx) {
        if (!_in_use[idx]) { return; }
        pop(idx);
        _in_use[idx] = false;
    }



    //
    // Touch
    // Move an index to the front.
    //
    void touch(size_t idx) {
        if (!_in_use[idx] || idx == _head) { return; }
        pop(idx);
        push(idx);
    }



    //
    // Evict
    // Remove the least-recently used node (the tail).
    //
    size_t evict() {
        size_t idx = _tail;
        erase(idx);
        return idx;
    }



    //
    // Pop
    // Remove a node from the list.
    //
    void pop(size_t idx) {
        size_t prev = _nodes[idx].prev;
        size_t next = _nodes[idx].next;
        if (prev != SIZE_MAX) {
            _nodes[prev].next = next;
        }
        if (next != SIZE_MAX) {
            _nodes[next].prev = prev;
        }
        if (_head == idx) {
            _head = next;
        }
        if (_tail == idx) {
            _tail = prev;
        }
    }



    //
    // Push
    // Add a node to the front of the list.
    //
    void push(size_t idx) {
        _nodes[idx].prev = SIZE_MAX;
        _nodes[idx].next = _head;
        if (_head != SIZE_MAX) _nodes[_head].prev = idx;
        _head = idx;
        if (_tail == SIZE_MAX) _tail = idx;
    }



    //
    // Object Size
    // Deeply scan the object, including pool and buffers.
    //
    size_t deep_size() const {
        size_t total = 0;
        total += sizeof(*this);
        total += _nodes.size() * sizeof(Node);
        total += (_in_use.size() + 7) / 8;
        return total;
    }
};




//
// Forward-Looking Cache Key
// Stores the bytes and equality operator.  Also leans on the Absl helper for hashing.
//
static constexpr size_t FLCKeySize = 24;
template<AnySupportedIntegral T>
class FLCKey {
    private:
    std::array<uint8_t, FLCKeySize> _bytes{};



    public:
    //
    // Constructors
    //
    FLCKey() = default;
    FLCKey(const T& value) {
        serialize(value);
    }



    //
    // Equality Operator
    //
    bool operator==(const FLCKey& other) const noexcept {
        return _bytes == other._bytes;
    }



    //
    // Use Abseil's Hashing Helper
    // Absl can now see the equality operator and hash function.
    //
    template<typename H>
    friend H AbslHashValue(H hash, const FLCKey<T>& k) {
        return H::combine_contiguous(std::move(hash), k._bytes.data(), k._bytes.size());
    }



    //
    // Serializer
    // Compute the bytes of an integral or mpz_class.  Endianness doesn't matter since all type T are the same within an instance.
    //
    void serialize(const T& value) {
        if constexpr (BuiltinIntegral<T>) {
            for (size_t i = 0; i < sizeof(T); ++i) {
                _bytes[FLCKeySize - 1 - i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
            }
        } else if constexpr(GMPIntegral<T>) {
            size_t count = 0;
            mpz_export(_bytes.data(), &count, 1, 1, 1, 0, value.get_mpz_t());
            if (count > FLCKeySize) {
                throw std::overflow_error("FLCKey serialize overflow from mpz_export.");
            }
            // Zero-fill rest if count < FLCKeySize
            std::fill(_bytes.begin() + count, _bytes.end(), 0);
        }
    }
};




//
// Forward-Looking Cache
// This class is NOT thread-safe!
//
template<AnySupportedIntegral T>
class ForwardLookingCache {
    private:
    absl::flat_hash_map<FLCKey<T>, size_t> _map;
    std::vector<FLCKey<T>> _entries;
    LRUTracker<FLCKey<T>> _lru;
    size_t _evictions;
    size_t _hits;
    size_t _size;
    size_t _max_entries;
    size_t _misses;
    size_t _new_insertions;
    size_t _overlap_insertions;



    public:
    //
    // Constructors
    //
    ForwardLookingCache(size_t max_entries) : _lru(max_entries) {
        init(max_entries);
    }



    //
    // Initialize
    // Builds the object, reusing it if necessary.
    //
    void init(size_t max_entries) {
        if (max_entries == 0) {
            throw std::logic_error("You can't have a forward-looking cache size of zero.");
        }
        _evictions = 0;
        _hits = 0;
        _size = 0;
        _max_entries = max_entries;
        _misses = 0;
        _new_insertions = 0;
        _overlap_insertions = 0;
        _lru.init(max_entries);
        _entries.clear();
        _entries.shrink_to_fit();
        _entries.resize(max_entries);
    }



    //
    // Getters
    //
    size_t get_evictions() const { return _evictions; }
    size_t get_hits() const { return _hits; }
    size_t get_max_entries() const { return _max_entries; }
    size_t get_misses() const { return _misses; }
    size_t get_new_insertions() const { return _new_insertions; }
    size_t get_overlap_insertions() const { return _overlap_insertions; }
    size_t get_size() const { return _size; }
    const LRUTracker<FLCKey<T>> get_lru() const { return _lru; }



    //
    // Contains Key
    // Scans for a key and returns true if it exists, false if not.  Will update LRU if found, unless "track" is false.
    //
    bool contains(const FLCKey<T>& k, bool track = true) {
        // Not found means nothing to do.
        auto it = _map.find(k);
        if (it == _map.end()) {
            _misses++;
            return false;
        }

        // Found requires sending it to the front of LRU cache, if allowed.
        if (track) {
            _lru.touch(it->second);
            _hits++;
        }

        return true;
    }



    //
    // Insert Key
    // Inserts a key.  Returns true if successful, false if key already existed.
    //
    bool insert(const FLCKey<T>& key) {
        // If it already exists, just update LRU cache.
        auto it = _map.find(key);
        if (it != _map.end()) {
            _lru.touch(it->second);
            _overlap_insertions++;
            return false;
        }

        // Didn't exist.  Get an index, respecting eviction if necessary.
        size_t index;
        if (_size < _max_entries) {
            // Have room, so just add.
            index = _size++;
        } else {
            // Evict to make room.  Clear the map of whoever was evicted.
            index = _lru.evict();
            _map.erase(_entries[index]);
            _evictions++;
        }

        // Now set the key into the map and track it.
        _entries[index] = key;
        _map[key] = index;
        _lru.insert(index);
        _new_insertions++;

        // Return successful insert.
        return true;
    }



    //
    // Object Size
    // Deeply scan the object, including pool and buffers.
    //
    size_t deep_size() const {
        size_t total = sizeof(*this);
        total += _lru.deep_size();
        total += _map.size() * (sizeof(FLCKey<T>) + sizeof(size_t));
        total += _entries.size() * sizeof(FLCKey<T>);

        return total;
    }

};
