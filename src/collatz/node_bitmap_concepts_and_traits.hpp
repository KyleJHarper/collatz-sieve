#pragma once

#include <concepts>
#include "concepts.hpp"
#include <vector>



//
// NodeBitmapBackend
// Going to try to use a concept to enforce a design contract for the two implementations.  We'll see how it goes.
//
template<typename Impl, typename T>
concept NodeBitmapBackend = requires(Impl impl, const Impl cimpl, const T& v1, const T& v2) {
    { impl.add(v1) } -> std::same_as<void>;
    { impl.add_range(v1, v2) } -> std::same_as<void>;
    { cimpl.contains(v1) } -> std::same_as<bool>;
    { impl.optimize() } -> std::same_as<void>;
};




//
// BitmapKeyTraits
// Helpers to handle prefix and suffix keys for the hash map and the trie node (prefix) chunks.
//
template<AnySupportedIntegral T>
struct BitmapKeyTraits {
    // Suffix can be anything, I suppose, but CRoaring recommends 32 bits.  We'll hard code that.
    using suffix_t = uint32_t;
    static_assert(sizeof(suffix_t) <= sizeof(unsigned long), "Suffix size (suffix_t) must not exceed mpz_t.get_ui(), which is unsigned long.");
    static constexpr size_t SUFFIX_BYTES = sizeof(suffix_t);
    static constexpr size_t SUFFIX_BITS = sizeof(suffix_t) * 8;
    static constexpr suffix_t SUFFIX_MASK = std::numeric_limits<suffix_t>::max();

    // Prefix is going to be fixed instead of a bunch of complicated logic.
    //   mpz_class -> mpz_class because only it can handle arbitrarily large numbers
    //   uint64_t or lower -> uint32_t because SUFFIX is hard-coded to 32 bits
    //   uint128_t -> uint128_t because we only need 96 bits but there's no uint96_t
    using prefix_t = std::conditional_t<
        std::same_as<mpz_class, T>
        , mpz_class
        , std::conditional_t<
            (sizeof(T) <= 8)
            , uint32_t
            , uint128_t
        >
    >;

    // Prefix chunking happens in Trie structures, and we'll keep them consistent here.
    // Obviously, the prefix size affects depth for large spaces.  We'll start with a hard-coded 32-bit and go from there.
    using prefix_chunk_t = uint32_t;
    static constexpr size_t PREFIX_CHUNK_BYTES = sizeof(prefix_chunk_t);
    static constexpr size_t PREFIX_CHUNK_BITS = sizeof(prefix_chunk_t) * 8;
    static constexpr size_t PREFIX_CHUNK_COUNT = sizeof(T) <= SUFFIX_BYTES ? 0 : (sizeof(T) - SUFFIX_BYTES + (PREFIX_CHUNK_BYTES - 1)) / PREFIX_CHUNK_BYTES;



    //
    // Get Prefix
    // Return the prefix for any type T.  When mpz_class, TLS.  See selection for prefix_t above.
    //
    static prefix_t get_prefix(const T& value) {
        if constexpr (BuiltinIntegral<T>) {
            if constexpr (sizeof(T) <= SUFFIX_BYTES) {
                return 0;
            } else {
                return static_cast<prefix_t>(value >> SUFFIX_BITS);
            }
        } else {
            static thread_local mpz_class tmp;
            mpz_fdiv_q_2exp(tmp.get_mpz_t(), value.get_mpz_t(), SUFFIX_BITS);
            return tmp;
        }
    }
    //
    // And an out version.
    static void get_prefix(const T& value, prefix_t& out) {
        if constexpr (BuiltinIntegral<T>) {
            if constexpr (sizeof(T) <= SUFFIX_BYTES) {
                out = 0;
            } else {
                out = static_cast<prefix_t>(value >> SUFFIX_BITS);
            }
        } else {
            mpz_fdiv_q_2exp(out.get_mpz_t(), value.get_mpz_t(), SUFFIX_BITS);
        }
    }



    //
    // Get Prefix Chunks
    // Return the prefix chunks whenever a break-apart of them is needed, such as use in the Trie.
    //
    constexpr std::array<prefix_chunk_t, PREFIX_CHUNK_COUNT> get_prefix_chunks_fixed_width(const T& value) {
        std::array<prefix_chunk_t, PREFIX_CHUNK_COUNT> results{};

        // Loop through with forward indexing but reverse (high-bit) reading/assignment into lower indexes to match for_each version.
        for (size_t i = 0; i < PREFIX_CHUNK_COUNT ; i++) {
            size_t shift = (PREFIX_CHUNK_COUNT - i) * PREFIX_CHUNK_BITS;
            results[i] = static_cast<prefix_chunk_t>(value >> shift);
        }

        return results;
    }
    //
    // Now a variable width version for mpz_class.  Uses TLS.
    static std::vector<prefix_chunk_t> get_prefix_chunks_variable_width(const T& value) {
        thread_local std::vector<prefix_chunk_t> results;
        size_t mpz_total_bits = mpz_size(value.get_mpz_t()) * GMP_LIMB_BITS;
        size_t mpz_chunk_count = (mpz_total_bits + PREFIX_CHUNK_BITS - 1) / PREFIX_CHUNK_BITS;
        results.resize(mpz_chunk_count);
        size_t count = 0;
        mpz_export(results.data(), &count, 1, PREFIX_CHUNK_BYTES, 1, 0, value.get_mpz_t());
        results.pop_back();
        return results;
    }



    //
    // For-Each Prefix Chunk
    // Iterator over chunks one at a time with a stop condition.
    //
    // Caller MUST return true to stop or false to continue.
    //
    template<typename Func>
    static void for_each_prefix_chunk(const T& value, Func&& callback) {
        static_assert(std::is_invocable_r_v<bool, Func, prefix_chunk_t>, "Callback must accept prefix_chunk_t and return bool");

        if constexpr(BuiltinIntegral<T>) {
            // Loop through getting highest bits first, then lower ones.
            for (size_t i = 0; i < PREFIX_CHUNK_COUNT; i++) {
                size_t shift = (PREFIX_CHUNK_COUNT - i) * PREFIX_CHUNK_BITS;
                prefix_chunk_t chunk = static_cast<prefix_chunk_t>(value >> shift);
                bool stop = callback(chunk);
                if (stop) { return; }
            }
        } else {
            // Use mpz_export to return all the data we need, including counts.
            thread_local std::vector<prefix_chunk_t> mpz_chunks;
            size_t mpz_total_bits = mpz_size(value.get_mpz_t()) * GMP_LIMB_BITS;
            size_t mpz_chunk_count = (mpz_total_bits + PREFIX_CHUNK_BITS - 1) / PREFIX_CHUNK_BITS;
            mpz_chunks.resize(mpz_chunk_count);
            size_t count = 0;
            mpz_export(mpz_chunks.data(), &count, 1, PREFIX_CHUNK_BYTES, 1, 0, value.get_mpz_t());
            for (size_t i = 0; i + 1 < count; i++) {
                bool stop = callback(mpz_chunks[i]);
                if (stop) { return; }
            }
        }
    }



    //
    // Get Suffix
    // Return the suffix for any type T.  Always suffix_t.
    //
    static suffix_t get_suffix(const T& value) {
        if constexpr (BuiltinIntegral<T>) {
            return static_cast<suffix_t>(value & SUFFIX_MASK);
        } else {
            return static_cast<suffix_t>(value.get_ui());
        }
    }

};
