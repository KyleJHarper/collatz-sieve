#pragma once

#include "concepts.hpp"
#include "for_each.hpp"
#include "node_bitmap_flat_hash.hpp"




/**
* @class NodeBitmap
* @brief The NodeBitmap facade.  Currently, only one implementation exists and is the default.
* @note This class follows CRoaring techniques, including half-open vs closed variants and such.  Pay attention.
* @tparam T Any supported integral (see concepts.hpp).
* @tparam BackendType The implementation to use.  Currently only `FlatHashBitmapImpl` exists and is the default.
*/
template<AnySupportedIntegral T, typename BackendType = FlatHashBitmapImpl<T>>
class NodeBitmap {
    private:
    /// @brief The static implementation object for this facade.
    BackendType _impl;



    public:
    /// @name Lifecycle Management
    /// @{

    /// @brief Default constructor.
    NodeBitmap() = default;



    /// @brief Disallow copying.
    NodeBitmap(const NodeBitmap&) = delete;
    /// @brief Disallow copying.
    NodeBitmap& operator=(const NodeBitmap&) = delete;



    /// @brief Moving is okay.
    NodeBitmap(NodeBitmap&&) noexcept = default;
    /// @brief Moving is okay.
    NodeBitmap& operator=(NodeBitmap&&) noexcept = default;



    /// @brief Invoke the implementation's clear method, wiping it out and freeing memory.
    void clear() { _impl.clear(); }

    /// @}


    /// @name Accessors
    /// @{

    /// @brief Get a readonly reference to the implementation object.
    const BackendType& get_impl() const { return _impl; }

    /// @brief Get a readonly reference to the implementation's map.
    /// @warning This is fragile, as it is tied directly to implementation-specific logic without a `requires` or safety check.
    const auto& get_map() const { return _impl.get_map(); }

    /// @}



    /// @brief Add a value to the bitmap.
    void add(const T& value) { _impl.add(value); }



    /**
    * @brief Add a contiguous range of values using the most optimized approach.
    * @note This is the Half-Open range [start, end).  Meaning, start is included but end isn't.
    */
    void add_range(const T& start, const T& end) { _impl.add_range(start, end); }



    /**
    * @brief Add a closed, contiguous range of values using the most optimized approach.
    * @note This is the Closed range [start, end].  Meaning, start and end are both included.
    */
    void add_range_closed(const T& start, const T& end) { _impl.add_range_closed(start, end); }



    /**
    * @brief Adds many sparse values at once.  May span prefixes.  Order not required.  Impl will sort for you.
    * @param count The number of elements of type `T` in `values`.
    * @param values The memory location with values to add.
    */
    void add_many(const size_t count, T* values) { _impl.add_many(count, values); }



    /**
    * @brief Check for a value to exist (be "on") in the bitmap.
    * @param value The value to search for.
    * @return True if present ("on"), false otherwise.
    */
    bool contains(const T& value) const { return _impl.contains(value); }



    /// @brief Removes an item from the bitmap.
    void remove(const T& value) { _impl.remove(value); }



    /// @brief Returns the smallest value in the NodeBitmap.
    T minimum() const { return _impl.minimum(); }



    /// @brief Returns the largest value in the NodeBitmap.
    T maximum() const { return _impl.maximum(); }



    /**
    * @brief Calculate the cardinality (count) of nodes turned "on".
    * @return The count of "on" nodes, typed to caller's `T`.
    */
    T cardinality() const { return _impl.cardinality(); }



    /// @brief Returns true if the bitmap is empty, false otherwise.
    bool empty() const { return _impl.empty(); }



    /**
    * @brief Merge (bitwise OR) another `BackendType` into this one.
    * @param src Another `BackendType` of the exact same `T`.
    */
    void merge(const NodeBitmap<T, BackendType>& src) { _impl.merge(src._impl); }



    /**
    * @brief An opertor which calls `merge()`.  This is a convenience to do: `my_bitmap |= another_bitmap`.
    * @note Unlike `merge()`, caller receives `*this` returned, not void.
    * @return This object, as required by `operator|=`.
    */
    NodeBitmap<T, BackendType>& operator|=(const NodeBitmap<T, BackendType>& src) { _impl |= src._impl; return *this; }



    /// @brief Copy source bitmap and metadata into self, making an exact copy.
    void clone(const NodeBitmap<T, BackendType>& src) { _impl.clone(src.get_impl()); }



    /**
    * @brief Calls any available optimization functions on internal members to free up wasted memory.
    */
    void optimize() { _impl.optimize(); }



    /**
    * @brief Calls the shrinkToFit() method for each bitmap and sums them.
    * @warning This is fragile because it is linked to implementation-specific behavior and not generic.
    * @returns The amount of space reclaimed, because that's what CRoaring's API does too.
    */
    size_t shrink_to_fit() { return _impl.shrink_to_fit(); }



    /**
    * @brief Calculate the size of this data structure as closely as possible.
    * @note This is not always cheap.  It requires iteration of all structures.
    * @return The size in bytes of the object.
    */
    size_t deep_size() const { return _impl.deep_size(); }



    /**
    * @brief Compare two BackendTypes' internals and return true if identical.
    *
    * This facade method currently just calls the implementation directly because there is no shared logic.
    *
    * This function checks:
    *   1. Implementation-specific details.
    *
    * @param first The first bitmap to compare.
    * @param second The second bitmap to compare.
    * @param err Pointer to a string where inequality or error messages are stored.
    * @return True if equal, false otherwise.
    */
    static bool st_equal(const NodeBitmap<T, BackendType>& first, const NodeBitmap<T, BackendType>& second, std::string* err = nullptr) { return BackendType::st_equal(first.get_impl(), second.get_impl(), err); }



    /**
    * @brief Compare another bitmap of `BackendType` to this one.
    *
    * This is a member helper which simply forwards to `BackendType::st_equal()`.
    *
    * @param second The second bitmap to compare against this.
    * @param err Pointer to a string where inequality or error messages are stored.
    * @return True if equal, false otherwise.
    */
    bool equal(const NodeBitmap<T, BackendType>& second, std::string* err = nullptr) const { return BackendType::st_equal(_impl, second.get_impl(), err); }



    /**
    * @brief Serialize the bitmap specifics of this object for export.
    *
    * Serialization happens in this order:
    *   1. Implementation-specific items.
    *
    * @note Since only one implemention exists, all work is handed off to it.  I.e.: there's no generalized data to control.
    * @note This method does not throw.
    * @param out The stream to write data to.
    * @param err Pointer to a string where errors, if any, are written.
    * @return A boolean indicating success or failure.  Do not discard.
    */
    [[nodiscard]] bool serialize(std::ostream& out, std::string* err = nullptr) const { return _impl.serialize(out, err); }



    /**
    * @brief Deserialize this object for import following a previous `serialize()`.
    *
    * Deserialization happens in the same order as serialization, obviously.
    *
    * @note This method does not throw.
    * @param in The stream to read data from.
    * @param err Pointer to a string where errors, if any, are written.
    * @return A boolean indicating success or failure.  Do not discard.
    */
    [[nodiscard]] bool deserialize(std::istream& in, std::string* err = nullptr) { return _impl.deserialize(in, err); }



    /**
    * @brief A for-each iterator returning each value in the bitmap to `callback`.
    *
    * This method uses `for_each_value_with_tls` when TLS storage isn't needed.  It applies `callback` to all values.
    *
    * Callback must have this signature: `(T& value)`.  Return type must be `ForEachSignal`.
    *
    * The ref (`T&`) prevents GMP allocations when `T` is an `mpz_class` by hoisting the internals and reconstituting the value on
    * each iteration.  This means the caller is free to modify the value at-will without affecting iteration or causing alloc()
    * storms.
    *
    * @warning Caller may NOT modify this bitmap while iterating!  It relies on CRoaring's iterators and internal structures, which
    * are invalidated upon changes.
    *
    * @tparam Func A function signature defined to match `callback`.
    * @param policy The desired policy (currently either Serial or Parallel) for processing.  See for_each_policy.hpp.
    * @param callback Method to invoke on each value.  Must return a `ForEachSignal`.
    */
    template<typename Func>
    void for_each_value(ForEachPolicy policy, Func&& callback) const {
        _impl.for_each_value(policy, callback);
    }



    /**
    * @brief A for-each iterator allowing callbacks with thread-local storage for transformation.
    *
    * Applies `callback` to all values according to the ForEachPolicy (serial or parallel) requested.  When serial, order is
    * guaranteed.
    *
    * Callback must have this signature: `(T& value, TLS_Type& tls)`.  Return type must be `ForEachSignal`.
    *
    * The ref (`T&`) prevents GMP allocations when `T` is an `mpz_class` by hoisting the internals and reconstituting the value on
    * each iteration.  This means the caller is free to modify the value at-will without affecting iteration or causing alloc()
    * storms.
    *
    * @warning Caller may NOT modify this bitmap while iterating!  It relies on CRoaring's iterators and internal structures, which
    * are invalidated upon changes.
    *
    * @tparam Func A function signature defined to match `callback`.
    * @tparam TLS_Type User-selected data type for the vector of thread-local storage to utilize.
    * @param policy The desired policy (currently either Serial or Parallel) for processing.  See for_each_policy.hpp.
    * @param tls A vector to store thread-local data in during callbacks.  This method WILL call `tls.resize()` if the number of
    * available threads reported by `omp_get_max_threads()` exceeds `tls.capacity()`.  From there, each thread is given a slice
    * (indexed element) of that vector.  This storage may be modified at-will in caller's `callback`.
    * @param callback Method to invoke on each value.  Must return a `ForEachSignal`.
    */
    template<typename Func, typename TLS_Type>
    void for_each_value_with_tls(ForEachPolicy policy, std::vector<TLS_Type>& tls, Func&& callback) const {
        _impl.for_each_value_with_tls(policy, tls, callback);
    }
};
