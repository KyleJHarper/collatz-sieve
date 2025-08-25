#pragma once
#include "absl/container/flat_hash_map.h"
#include <array>
#include <cstdint>
#include <list>
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
// Forward-Looking Cache Key
// Stores the bytes and equality operator.  Also leans on the Absl helper for hashing.
//
static constexpr size_t FLCKeySize = 24;
template<IntegralOrMPZClass T>
class FLCKey {
    private:
    std::array<uint8_t, FLCKeySize> _bytes{};



    public:
    //
    // Constructors
    //
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
        if constexpr (std::integral<T>) {
            for (size_t i = 0; i < sizeof(T); ++i) {
                _bytes[FLCKeySize - 1 - i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
            }
        } else {
            size_t count = 0;
            mpz_export(_bytes.data(), &count, 1, 1, 1, 0, value.get_mpz_t());
            // Zero-fill rest if count < FLCKeySize
            std::fill(_bytes.begin() + count, _bytes.end(), 0);
        }
    }
};




//
// Forward-Looking Cache
// This class is NOT thread-safe!
//
template<IntegralOrMPZClass T>
class ForwardLookingCache {
    // Absl needs a typename here for clarity to compile.
    using List_T      = std::list<FLCKey<T>>;
    using Iterator_T  = typename List_T::iterator;

    private:
    size_t _evictions;
    size_t _hits;
    size_t _max_entries;
    size_t _misses;
    size_t _new_insertions;
    size_t _overlap_insertions;
    List_T _lru_list;
    absl::flat_hash_map<FLCKey<T>, Iterator_T> _set;



    public:
    //
    // Constructors
    //
    ForwardLookingCache(size_t max_entries) {
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
        _max_entries = max_entries;
        _misses = 0;
        _new_insertions = 0;
        _overlap_insertions = 0;
        _lru_list.clear();
        _set.clear();
        // Add a little extra padding to (hopefully) avoid as much rehashing.
        _set.reserve(static_cast<size_t>(max_entries * 1.25));
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



    //
    // Contains Key
    // Scans for a key and returns true if it exists, false if not.  Will update LRU if found, unless "track" is false.
    //
    bool contains(const FLCKey<T>& k, bool track = true) {
        // Not found means nothing to do.
        auto it = _set.find(k);
        if (it == _set.end()) {
            _misses++;
            return false;
        }

        // Found requires sending it to the front of LRU cache, if allowed.
        if (track) {
            _lru_list.splice(_lru_list.begin(), _lru_list, it->second);
            it->second = _lru_list.begin();
            _hits++;
        }
        return true;
    }



    //
    // Insert Key
    // Inserts a key.  Returns true if successful, false if key already existed.
    //
    bool insert(const FLCKey<T>& k) {
        // If it already exists, just update LRU cache.
        auto it = _set.find(k);
        if (it != _set.end()) {
            _lru_list.splice(_lru_list.begin(), _lru_list, it->second);
            it->second = _lru_list.begin();
            _overlap_insertions++;
            return false;
        }

        // Didn't exist.  Add it and track with LRU.
        _lru_list.push_front(k);
        _set.emplace(k, _lru_list.begin());
        _new_insertions++;

        // Clean up if we're over sized now.
        if (_set.size() > _max_entries) {
            // Evict LRU
            FLCKey<T> lru = _lru_list.back();
            _set.erase(lru);
            _lru_list.pop_back();
            _evictions++;
        }

        // Return successful insert.
        return true;
    }



    //
    // Object Size
    // Deeply scan the object, including pool and buffers.
    //
    size_t deep_size() const {
        size_t total = sizeof(*this);
        // list nodes: estimate node size = sizeof(FLCKey) + pointer overhead.
        // std::list node typically contains 2 pointers (prev, next) + value.
        const size_t list_node_overhead = sizeof(void*) * 2;
        total += _lru_list.size() * (sizeof(FLCKey<T>) + list_node_overhead);

        // map elements: estimate per-element size (key + value + overhead)
        // flat_hash_map stores keys/values in contiguous bucket storage; approximate as pair size * size
        total += _set.size() * (sizeof(FLCKey<T>) + sizeof(Iterator_T));

        // bucket overhead: approximate using bucket count (if available)
        // absl::flat_hash_map doesn't have bucket_count(); approximate extra overhead:
        total += _set.size() / 8 + 64; // small fudge-factor for buckets/metadata

        return total;
    }

};
