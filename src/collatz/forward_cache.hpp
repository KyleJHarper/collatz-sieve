#pragma once
#include "absl/container/flat_hash_map.h"
#include <array>
#include <cstdint>
#include <list>





//
// Build a cache device to track value's we've seen.
///???????????????????????????????????????????/
//



static constexpr size_t FLCKeySize = 24;

struct FLCKey {
    std::array<uint8_t, FLCKeySize> bytes{};

    bool operator==(const FLCKey& other) const noexcept {
        return bytes == other.bytes;
    }

    // Use Abseil's Hashing Helper
    template <typename H>
    friend H AbslHashValue(H h, const FLCKey& k) {
        return H::combine_contiguous(std::move(h), k.bytes.data(), k.bytes.size());
    }
};



class ForwardLookingCache {
    private:
    size_t _evictions;
    size_t _hits;
    size_t _max_entries;
    size_t _misses;
    size_t _new_insertions;
    size_t _overlap_insertions;
    std::list<FLCKey> _lru_list;
    absl::flat_hash_map<FLCKey, std::list<FLCKey>::iterator> _set;



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
        _evictions = 0;
        _hits = 0;
        _max_entries = max_entries;
        _misses = 0;
        _new_insertions = 0;
        _overlap_insertions = 0;
        _lru_list.clear();
        _set.clear();
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
    // Scans for a key and return strue if it exists, false if not.
    //
    bool contains(const FLCKey& k) {
        // Not found means nothing to do.
        auto it = _set.find(k);
        if (it == _set.end()) {
            _misses++;
            return false;
        }

        // Found requires sending it to the front of LRU cache.
        _lru_list.splice(_lru_list.begin(), _lru_list, it->second);
        it->second = _lru_list.begin();
        _hits++;
        return true;
    }



    //
    // Insert Key
    // Inserts a key.  Returns true if successful, false if key already existed.
    //
    bool insert(const FLCKey& k) {
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
        _set[k] = _lru_list.begin();
        _new_insertions++;

        // Clean up if we're over sized now.
        if (_set.size() > _max_entries) {
            // Evict LRU
            FLCKey lru = _lru_list.back();
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
        //TODO
        // Done
        return total;
    }

};
