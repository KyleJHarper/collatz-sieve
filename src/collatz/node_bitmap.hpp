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

    const auto& get_map() const { return _impl.get_map(); }
    void add(const T& value) { _impl.add(value); }
    void add_range(const T& start, const T& end) { _impl.add_range(start, end); };
    void add_range_closed(const T& start, const T& end) { _impl.add_range_closed(start, end); };
    bool contains(const T& value) const { return _impl.contains(value); }
    void remove(const T& value) { _impl.remove(value); }
    T cardinality() const { _impl.cardinality(); }
    void clear() { _impl.clear(); }
    void clone(const FlatHashBitmapImpl<T>& src) { _impl.clone(src); }
    void optimize() { _impl.optimize(); };
    size_t shrink_to_fit() { return _impl.shrink_to_fit(); }
    size_t deep_size() const { return _impl.deep_size(); }

    template<typename Func, typename TLS_Type>
    void for_each_transformer(BitmapTransformerPolicy policy, std::vector<TLS_Type>& tls, Func&& callback) { _impl.for_each_transformer(policy, tls, callback); }
    template<typename Func>
    void for_each_value(BitmapTransformerPolicy policy, Func&& callback) { _impl.for_each_value(policy, callback); }
};
