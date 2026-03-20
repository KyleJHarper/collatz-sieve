#pragma once

#include "concepts.hpp"
#include "node_bitmap_flat_hash.hpp"
#include "node_bitmap_trie.hpp"




//
// The NodeBitmap facade.
//
template<AnySupportedIntegral T>
class NodeBitmap {
    private:
    using Impl = std::conditional_t<
        BuiltinIntegral<T>
        , FlatHashBitmapImpl<T>
        , TrieBitmapImpl<T>
    >;
    Impl _impl;
    static_assert(NodeBitmapBackend<Impl, T>, "Bitmap backend doesn't match the interface required.");


    public:
    NodeBitmap() = default;

    void add(const T& value) { _impl.add(value); }
    void add_range(const T& start, const T& end) { _impl.add_range(start, end); };
    bool contains(const T& value) const { return _impl.contains(value); }
    void optimize() { _impl.optimize(); };
};
