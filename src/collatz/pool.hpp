#pragma once

#include <vector>
#include <cstddef>
#include <new>
#include <utility>
#include <mutex>
#include <stack>


//
// A Generic Object Factory .  Because I'm a glutton for punishment.
// We will build a vector of blocks and grow as needed.  The idea behind this is to allow parallel
// threads to get their own thread_local version.
//
// This is NOT THREAD SAFE!  Use the ThreadSafeObjectFactory for that.
//
template<typename T>
class ObjectPool {
    private:
    const size_t _block_size;
    size_t _current_index;
    std::vector<T*> _blocks;
    std::stack<T*> _free_list;

    public:
    explicit ObjectPool(size_t block_size = 4096)
        : _block_size(block_size), _current_index(0) {
        _allocate_block();
    }
    ~ObjectPool() {
        for (auto& block : _blocks) {
            for (size_t i = 0; i < _block_size; i++) {
                block[i].~T();
            }
            operator delete(static_cast<void*>(block));
        }
    }

    template<typename... Args>
    T* create(Args&&... args) {
        if (!_free_list.empty()) {
            T* recycled = _free_list.top();
            _free_list.pop();
            return new (recycled) T(std::forward<Args>(args)...);
        }

        if (_current_index >= _block_size) {
            _allocate_block();
        }

        T* ptr = new(&_blocks.back()[_current_index]) T(std::forward<Args>(args)...);
        _current_index++;
        return ptr;
    }
    void destroy(T* obj) {
        if (obj) {
            obj->~T();
            _free_list.push(obj);
        }
    }

    private:
    void _allocate_block() {
        T* new_block = static_cast<T*>(operator new(_block_size * sizeof(T)));
        _blocks.push_back(new_block);
        _current_index = 0;
    }
};



//
// Using composition, we'll add a mutex and make a thread-safe factory.
//
template<typename T>
class ThreadSafeObjectPool {
    private:
    ObjectPool<T> _factory;
    std::mutex _mutex;

    public:
    // Constructor
    explicit ThreadSafeObjectPool(size_t block_size = 4096)
        : _factory(block_size) {}

    // Destructor
    ~ThreadSafeObjectPool() {
        std::lock_guard<std::mutex> lock(_mutex);
        // ObjectFactory<T>'s destructor runs here safely
    }


    template<typename... Args>
    T* create(Args&&... args) {
        std::lock_guard<std::mutex> lock(_mutex);
        return _factory.create(std::forward<Args>(args)...);
    }

};
