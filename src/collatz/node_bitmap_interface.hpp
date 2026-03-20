#pragma once
#include "concepts.hpp"
#include <concepts>
#include <gmpxx.h>




//
// Interface for bitmap implementations.
//
template<AnySupportedIntegral T>
class INodeBitmapBackend {
    private:
    // Set a suffix of 32 bits, which CRoaring recommends for its internal 16+16 strategy.
    using suffix_t = uint32_t;
    static constexpr suffix_t SUFFIX_BYTES = sizeof(suffix_t);
    static constexpr suffix_t SUFFIX_BITS = sizeof(suffix_t) * 8;
    static constexpr suffix_t SUFFIX_MASK = std::numeric_limits<suffix_t>::max();

    // Pick a prefix.
    // MPZ is arbitrary length and only works in trie.  Set to void.
    // 64-bit (8 bytes) and smaller get a 32-bit prefix, even if some is wasted.
    // 128-bit (16 bytes) gets a 128-bit prefix, even though only 96 is needed.
    using prefix_t = std::conditional_t<
        std::same_as<mpz_class, T>
        , void
        , std::conditional_t<
            (sizeof(T) <= 8)
            , uint32_t
            , uint128_t
        >
    >;
    // Trie types will need a prefix chunk type.
    using prefix_chunk_t = uint32_t;
    static constexpr size_t PREFIX_CHUNK_BITS = sizeof(prefix_chunk_t) * 8;
    static constexpr size_t PREFIX_CHUNK_BYTES = sizeof(prefix_chunk_t);
    static constexpr size_t PREFIX_CHUNK_COUNT = sizeof(T) <= SUFFIX_BYTES ? 0 : (sizeof(T) - SUFFIX_BYTES) / PREFIX_CHUNK_BYTES;

    public:
    virtual ~INodeBitmapBackend() = default;

    virtual void add(const T& value) = 0;
    virtual void add_range(const T& start, const T& end) = 0;
    virtual bool contains(const T& value) const = 0;
    virtual void optimize() = 0;
};
