#pragma once

#include "concepts.hpp"
#include "node_bitmap_flat_hash.hpp"




//
// The NodeBitmap facade.  It uses prefixes and suffixes (implementation-specific) to store values in a CRoaring bitmap.  The only
// implementation currently is a flat hash map for prefixes with a 32-bit suffix for the CRoaring bitmap.
//
// NOTE!  We will follow the CRoaring techniques, such as HALF-OPEN vs CLOSED variants and such.  Pay attention!
//
template<AnySupportedIntegral T>
class NodeBitmap {
    private:
    FlatHashBitmapImpl<T> _impl;


    public:
    NodeBitmap() = default;

    void add(const T& value) { _impl.add(value); }
    void add_range(const T& start, const T& end) { _impl.add_range(start, end); };
    void add_range_closed(const T& start, const T& end) { _impl.add_range_closed(start, end); };
    bool contains(const T& value) const { return _impl.contains(value); }
    void remove(const T& value) { _impl.remove(value); }
    void clear() { _impl.clear(); }
    void optimize() { _impl.optimize(); };
    size_t shrink_to_fit() { return _impl.shrink_to_fit(); }
};
