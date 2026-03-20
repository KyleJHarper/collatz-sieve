#pragma once

#include "concepts.hpp"
#include "node_bitmap_concepts_and_traits.hpp"
#include <gmp.h>
#include <roaring/roaring.hh>
#include <absl/container/flat_hash_map.h>




//
// Trie Bitmap
// An implementation of the roaring bitmap for our NodeBitmap, using a node structure (trie) to handle any arbitrary prefix size or
// quantity.  Mostly for mpz_class.  Probably not as performant for native/builtin types (fixed-width hashable by Abseil).
//
template<AnySupportedIntegral T>
class TrieBitmapImpl {
    private:
    using Traits = BitmapKeyTraits<T>;
    using prefix_t = Traits::prefix_t;
    using suffix_t = Traits::suffix_t;
    using prefix_chunk_t = Traits::prefix_chunk_t;


    // Design a struct for the node and keep it internally contained I suppose.
    struct TrieBitmapNode {
        absl::flat_hash_map<prefix_chunk_t, std::unique_ptr<TrieBitmapNode>> children;
        roaring::Roaring bitmap;
    };

    // Create a root node to start the tree.
    TrieBitmapNode _root;



    public:
    TrieBitmapImpl() = default;



    //
    // At
    // Mimic other containers' at methods.  Will find a node but not create any (readonly).
    // Returns nullptr if not found.
    //
    const TrieBitmapNode* at(const T& value) const {
        const TrieBitmapNode* node = &_root;
        bool prefix_missing = false;

        // Loop through all prefixes.  If any don't exist, the node clearly doesn't exist.
        Traits::for_each_prefix_chunk(value, [&](const prefix_chunk_t& prefix_chunk) {
            // Find the part in our current node.
            auto it = node->children.find(prefix_chunk);
            if (it == node->children.end()) {
                prefix_missing = true;
            } else {
                // Move to the next node.
                node = it->second.get();
            }
            return prefix_missing;
        });
        // If we gave up early due to a missing prefix, return to the caller.
        if (prefix_missing) { return nullptr; }

        // We found it.  Send it back.
        return node;
    }



    //
    // operator[]
    // Mimic other containers' [] operator.  Will find a node and create new ones if needed.
    //
    TrieBitmapNode& operator[](const T& value) {
        TrieBitmapNode* node = &_root;

        // Loop through all prefixes.  Add any missing nodes.
        Traits::for_each_prefix_chunk(value, [&](const prefix_chunk_t& prefix_chunk) {
            // Find the part in our current node.
            auto it = node->children.find(prefix_chunk);
            if (it == node->children.end()) {
                // It's missing.  Make the child, assign it to the node's map, then move the node to the child.
                TrieBitmapNode child = std::make_unique<TrieBitmapNode>();
                TrieBitmapNode* child_ptr = child.get();
                node->children[prefix_chunk] = std::move(child);
                node = child_ptr;
            } else {
                node = it->second.get();
            }
            return false;
        });

        // Return the final node.
        return node;
    }



    //
    // Add
    // Add a value to the bitmap.
    //
    void add(const T& value) {
        TrieBitmapNode* node = &(*this)[value];
        node->bitmap.add(Traits::get_suffix(value));
    }



    //
    // Contains
    // Check for a value to exist in the bitmap.
    //
    bool contains(const T& value) const {
        // Find the node.  Quit if it's missing.
        const TrieBitmapNode* node = at(value);
        if (node == nullptr) { return false; }

        // Suffix time.  If we're here, we found the terminal node of a full prefix.  Call the roaring contains() method.
        suffix_t suffix = Traits::get_suffix(value);
        return node->bitmap.contains(suffix);
    }



    //
    // Add Range
    // Add a contiguous range of values using the most optimized approach.  Only works well when the prefixes match.
    //
    void add_range(const T& start, const T& end) {
        prefix_t prefix_1 = Traits::get_prefix(start);
        prefix_t prefix_2 = Traits::get_prefix(end);
        if (prefix_1 != prefix_2) {
            // Have to do it manually.  Rip.
            for (T value = start; value < end; value++) {
                add(value);
            }
            return;
        }

        TrieBitmapNode* node = &(*this)[start];
        node->bitmap.addRange(Traits::get_suffix(start), Traits::get_suffix(end));
    }



    //
    // Optimize
    // Calls the internal runOptimize() for each roaring map, which (I think) is an RLE analysis and/or compaction task.
    //
    void optimize() {
        // Optimize the root.
        _root.bitmap.runOptimize();

        // Recurse to the children.
        for(auto& [prefix, child] : _root.children) {
            optimize_children(child);
        }
    }
    //
    // Helper to recurse.
    void optimize_children(TrieBitmapNode& parent) {
        // Recurse to the children.
        for(auto& [prefix, child] : parent->children) {
            child->bitmap.runOptimize();
            if (child->children.empty() == false) {
                optimize_children(child);
            }
        }
    }


};

