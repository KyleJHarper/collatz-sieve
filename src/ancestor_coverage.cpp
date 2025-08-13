#include <gmpxx.h>
#include <stdint.h>
#include <string>
#include "collatz/binary_tree.hpp"
#include "collatz/node.hpp"
#include "collatz/collatz.hpp"
#include "CLI11.hpp"
#include "logging.hpp"



int main(int argc, char **argv) {
    // Get logger.
    init_logger();
    // Process options.
    typedef uint64_t tree_type;
    size_t levels;
    size_t scan_levels;
    bool verbose = false;
    CLI::App options("Builds a BinaryTree and calculates the per-level and global coverage along the way.");
    options.add_option("-s,--scan", scan_levels, "Number of levels to scan for ancestors needing covered.")->default_val(4);
    options.add_option("-l,--levels", levels, "Number of levels to build the tree (in total).")->default_val(16);
    options.add_flag(
        "-v,--verbose"
        , [&](size_t x){if(x>0) {verbose=true; logger->set_level(spdlog::level::debug);}}
        , "Enable verbosity."
    );
    CLI11_PARSE(options, argc, argv);
    logger->debug("Selected options were:");
    logger->debug("  Levels: {}", levels);
    logger->debug("  Verbose: {}", verbose);

    // Build the tree.
    logger->info("Building tree with {} levels.", levels);
    BinaryTreeOptions opts;
    opts.preserve_ancestors = true;
    opts.pruned = false;
    opts.track_node_metadata = true;
    BinaryTree<tree_type> tree(levels, opts);

    // Scan.
    const Node<tree_type> *descendant;
    Collatz<tree_type> collatz;
    for (size_t level = 1; level < scan_levels; level++) {
        logger->debug("Scanning level {}", level);
        for (Node<tree_type>* node : tree.get_level_map().at(level)) {
            if (node->is_below_high_water_mark() || node->has_high_water_mark_ancestor()) {
                continue;
            }
            logger->debug("Node {} doesn't have a HWM ancestor and needs help.", node->get_value());
            descendant = node;
            collatz.init(node->get_value());
            while (true) {
                logger->info("starting loop");
                for (uint8_t i = 0; i < descendant->get_child_count(); i++) {
                    logger->info("trying child {}", i);
                    // If the child matches the parent's next step, it is the correct child.
                    const Node<tree_type>* child = descendant->get_child(i);
                    std::string node_full_pattern = collatz.get_oe_pattern_string();
                    std::string child_oe_chain = child->get_odd_even_chain_string();
                    // One (1) is a special case.
                    if (node->get_value() == 1) {
                        node_full_pattern += "EE";
                    }
                    logger->info("collatz.get_oe_pattern_string:    {}", node_full_pattern);
                    logger->info("child->get_odd_even_chain_string: {}", child_oe_chain);
                    if (node_full_pattern.starts_with(child_oe_chain)) {
                        logger->debug("  > found the right descendant at: {}", child->get_value());
                        descendant = child;
                        break;
                    }
                }
                if (descendant->is_below_high_water_mark()) {
                    logger->info("Found it");
                    break;
                }
            }
        }
    }

    // All done.
    return 0;
}
