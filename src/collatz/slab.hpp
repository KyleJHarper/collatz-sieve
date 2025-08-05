#pragma once

#include <vector>
#include <memory>
#include <cstddef>
#include <stdexcept>
#include <cassert>
#include <type_traits>


//
// Generic slab allocator to reduce allocation-heavy pools/lists/vectors/etc.
//
template <typename T>
class SlabAllocator {
    static_assert(!std::is_pointer<T>::value, "SlabAllocator should not be used for pointer types directly.");
    static_assert(std::is_default_constructible<T>::value, "T must be default-constructible for slab allocation.");

    private:
    size_t _slab_size;
    std::vector<std::unique_ptr<T[]>> _slabs;
    std::vector<T*> _free_list;
    std::vector<T*> _allocated_list;
    size_t _total_allocated = 0;

    public:
    // Constructor: slab size in elements (not bytes)
    explicit SlabAllocator(size_t slab_size = 1024)
        : _slab_size(slab_size) {
        if (_slab_size == 0) {
            throw std::invalid_argument("Slab size must be greater than zero.");
        }
    }

    // Destructor to help avoid double-free.
    ~SlabAllocator() {}

    // Make this non-copyable.
    SlabAllocator(const SlabAllocator&) = delete;
    SlabAllocator& operator=(const SlabAllocator&) = delete;
    SlabAllocator(SlabAllocator&&) = default;
    SlabAllocator& operator=(SlabAllocator&&) = default;

    // Allocate a new T object from the slab (uninitialized memory).
    T* allocate() {
        if (_free_list.empty()) {
            allocate_new_slab();
        }
        T* ptr = _free_list.back();
        _free_list.pop_back();
        _allocated_list.push_back(ptr);
        _total_allocated++;
        return ptr;
    }

    // Explicitly release a single object: call destructor and reclaim memory.
    void release(T* ptr) {
        if (ptr != nullptr) {
            ptr->~T();
            _free_list.push_back(ptr);

            // Remove ptr from _allocated_list
            auto it = std::find(_allocated_list.begin(), _allocated_list.end(), ptr);
            if (it != _allocated_list.end()) {
                _allocated_list.erase(it);
            }

            _total_allocated--;
        }
    }

    // Reset allocator but keep existing memory slabs.
    void reset() {
        for (T* ptr : _allocated_list) {
            ptr->~T();
            _free_list.push_back(ptr);
        }
        _allocated_list.clear();
        _total_allocated = 0;
    }

    // Stats accessors
    size_t slab_size() const {
        return _slab_size;
    }

    size_t slab_count() const {
        return _slabs.size();
    }

    size_t capacity() const {
        return _slabs.size() * _slab_size;
    }

    size_t allocated_count() const {
        return _total_allocated;
    }

    size_t free_count() const {
        return _free_list.size();
    }

    private:
    void allocate_new_slab() {
        std::unique_ptr<T[]> slab(new T[_slab_size]);
        for (size_t i = 0; i < _slab_size; ++i) {
            _free_list.push_back(&slab[i]);
        }
        _slabs.push_back(std::move(slab));
    }
};


//
// Thread-safe version.  Uses a simple mutex.  Probably not performant in threaded situations.
//
template <typename T>
class ThreadSafeSlabAllocator {
private:
    SlabAllocator<T> _base;
    mutable std::mutex _mutex;

public:
    explicit ThreadSafeSlabAllocator(size_t slab_size = 1024)
        : _base(slab_size) {}

    T* allocate() {
        std::lock_guard<std::mutex> lock(_mutex);
        return _base.allocate();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(_mutex);
        _base.clear();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(_mutex);
        _base.reset();
    }

    size_t slab_size() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _base.slab_size();
    }

    size_t slab_count() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _base.slab_count();
    }

    size_t capacity() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _base.capacity();
    }

    size_t allocated_count() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _base.allocated_count();
    }

    size_t free_count() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _base.free_count();
    }
};

