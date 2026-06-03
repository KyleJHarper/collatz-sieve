#include "collatz/binary_tree.hpp"
#include "collatz/collatz.hpp"


int main() {

    struct CoolMetadata {
        uint64_t total_processed = 0;
        uint64_t skipped = 0;
        // SomeCacheSystem cache;
    };
    BinaryTree<uint64_t> tree(32);
    tree.generate_value_map();
    std::vector<CoolMetadata> tls;
    tree.for_each_uncovered_value_with_tls(ForEachPolicy::PARALLEL, tls, [&](const uint64_t& value, CoolMetadata& my_tls) {
        // Check the LRU first.
        // if (my_tls.cache.contains(value)) {
        //     my_tls.skipped++;
        // } else {
        //     Collatz<uint64_t>::st_verify(value);
        //     my_tls.cache.add(value);
        // }
        Collatz<uint64_t>::st_verify(value);
        //sdf
        // Bump count, even if it was skipped.
        my_tls.total_processed++;

        // Let the iterator know it can continue.  Use ForEachSignal::BREAK to stop.
        if (value > (1ULL << 34)) {
            std::cout << "Reach a big enough value to be finished.  Value is: " << to_string_any(value) << std::endl;
            return ForEachSignal::BREAK;
        }
        return ForEachSignal::CONTINUE;
    });

}
