#include <gmpxx.h>
#include <stdint.h>
#include <string>
#include "collatz/binary_tree.hpp"
#include "collatz/concepts.hpp"
#include "collatz/node.hpp"
#include "collatz/collatz.hpp"
#include "CLI11.hpp"
#include "logging.hpp"

template<IntegralOrMPZClass T>
void run (size_t levels) {
    // Build the tree.
    logger->info("Building tree with {} levels.", levels);
    BinaryTreeOptions opts;
    opts.preserve_ancestors = true;
    opts.prune_hwm_nodes = false;
    opts.track_node_metadata = true;
    BinaryTree<T> tree(levels, opts);

    // Scan.
    // We don't need to build a whole tree and traverse it.  That'll just use a stupid amount of memory.
    // Instead, build children on-the-fly.
    std::vector<Node<T>*> lineage;
    for (size_t level = 1; level <= tree.get_level_count(); level++) {
        logger->debug("Scanning level {}", level);
        for (const Node<T>* node : tree.get_level_map().at(level)) {
            if (node->is_below_high_water_mark() || node->has_high_water_mark_ancestor()) {
                continue;
                // throw std::logic_error("Node " + std::to_string(node->get_value()) + " somehow survived purning but hit HWM or has ancestor...");
            }

            // Make a copy of the node so we don't break the tree.
            Node<T>* root = new Node<T>(node->get_value(), true);
            lineage.clear();
            lineage.push_back(root);
            Node<T>* descendant;
            Node<T>* child;
            std::string root_preamble = "";
            if constexpr(std::integral<T>) {
                root_preamble = "Node " + std::to_string(root->get_value()) + " (level " + std::to_string(level) + ")";
            } else {
                root_preamble = "Node " + root->get_value().get_str() + " (level " + std::to_string(level) + ")";
            }
            std::string root_full_fg_pattern = Collatz<T>::st_get_fg_pattern_string(root->get_value());
            // One (1) is a special case.
            if (root->get_value() == 1) {
                root_full_fg_pattern += "G";
            }
            std::string required_oe = Collatz<T>::fg_to_oe(root_full_fg_pattern, root->get_hwm_index(), false);
            logger->debug(
                "{} doesn't have a HWM ancestor and needs help:  Hits HWM at step {}.  Must match OE Chain: {}.  Full F-G Chain is: {}."
                , root_preamble
                , root->get_hwm_index() + 1
                , required_oe
                , root_full_fg_pattern
            );

            // Loop through lineage until we find the right child.
            bool found = false;
            while (! found) {
                descendant = lineage.back();
                descendant->spawn_children();
                for (uint8_t i = 0; i < descendant->get_child_count(); i++) {
                    // If the child matches the parent's next step, it is the correct child.
                    child = descendant->get_child_unsafe(i);
                    std::string child_fg_chain = child->get_fg_chain_string();
                    logger->debug("Trying child {} with value {} and FG chain: {}", i, child->get_value(), child_fg_chain);
                    if (root_full_fg_pattern.starts_with(child_fg_chain)) {
                        logger->debug("Child {} matches and joins the lineage.", child->get_value());
                        lineage.push_back(child);
                        if (child->is_below_high_water_mark()) {
                            logger->info(
                                "{} is covered by a descendant at {} (level {}) matching F-G chain {} ({}/{}, +{})."
                                , root_preamble
                                , child->get_value()
                                , child->get_level()
                                , child_fg_chain
                                , child->get_threes_value()
                                , child->get_twos_value()
                                , child->get_fg_constant()
                            );
                            size_t chain_width = lineage.back()->get_fg_chain_length();
                            size_t numeric_width = 0;
                            if constexpr(std::integral<T>) {
                                numeric_width = std::to_string(lineage.back()->get_value()).length();
                            } else {
                                numeric_width = lineage.back()->get_value().get_str().length();
                            }
                            for (const Node<T>* ancestor : lineage) {
                                std::string direction = "";
                                if (ancestor != root) {
                                    direction = ancestor->get_parent()->get_child(0)->get_value() == ancestor->get_value() ? "L" : "R";
                                }
                                // FMT gets weird with GMP, even with our custom stuff in logging.hpp/cpp.  Just stringify it.
                                std::string value = "";
                                if constexpr(std::integral<T>) {
                                    value = std::to_string(ancestor->get_value());
                                } else {
                                    value = ancestor->get_value().get_str();
                                }
                                logger->info(
                                    "  -> {:<{}}  {:>{}}  {}"
                                    , ancestor->get_fg_chain_string()
                                    , chain_width
                                    , value
                                    , numeric_width
                                    , direction
                                );
                            }
                            found = true;
                        }
                        break;
                    }
                }
            }
            // Clean up the root object, which will cascade.
            delete root;
        }
    }
}



int main(int argc, char **argv) {
    // Get logger.
    init_logger();
    // Process options.
    typedef mpz_class my_type;
    // typedef uint64_t my_type;
    size_t levels;
    bool verbose = false;
    CLI::App options("Builds a BinaryTree and calculates the per-level and global coverage along the way.");
    options.add_option("-l,--levels", levels, "Number of levels to scan.")->default_val(4);
    options.add_flag(
        "-v,--verbose"
        , [&](size_t x){if(x>0) {verbose=true; logger->set_level(spdlog::level::debug);}}
        , "Enable verbosity."
    );
    CLI11_PARSE(options, argc, argv);
    logger->debug("Selected options were:");
    logger->debug("  Levels: {}", levels);
    logger->debug("  Verbose: {}", verbose);

    run<my_type>(levels);
    // // Build the tree.
    // logger->info("Building tree with {} levels.", levels);
    // BinaryTreeOptions opts;
    // opts.preserve_ancestors = true;
    // opts.pruned = false;
    // opts.track_node_metadata = true;
    // BinaryTree<my_type> tree(levels, opts);

    // // Scan.
    // // We don't need to build a whole tree and traverse it.  That'll just use a stupid amount of memory.
    // // Instead, build children on-the-fly.
    // std::vector<Node<my_type>*> lineage;
    // for (size_t level = 1; level <= tree.get_level_count(); level++) {
    //     logger->debug("Scanning level {}", level);
    //     for (const Node<my_type>* node : tree.get_level_map().at(level)) {
    //         if (node->is_below_high_water_mark() || node->has_high_water_mark_ancestor()) {
    //             continue;
    //             // throw std::logic_error("Node " + std::to_string(node->get_value()) + " somehow survived purning but hit HWM or has ancestor...");
    //         }

    //         // Make a copy of the node so we don't break the tree.
    //         Node<my_type>* root = new Node<my_type>(node->get_value(), true);
    //         lineage.clear();
    //         lineage.push_back(root);

    //         Node<my_type>* descendant;
    //         Node<my_type>* child;
    //         std::string root_preamble = "";
    //         if constexpr(std::integral<my_type>) {
    //             root_preamble = "Node " + std::to_string(root->get_value()) + " (level " + std::to_string(level) + ")";
    //         } else {
    //             root_preamble = "Node " + root->get_value().get_str() + " (level " + std::to_string(level) + ")";
    //         }
    //         std::string root_full_fg_pattern = Collatz<my_type>::st_get_fg_pattern_string(root->get_value());
    //         // One (1) is a special case.
    //         if (root->get_value() == 1) {
    //             root_full_fg_pattern += "G";
    //         }
    //         std::string required_oe = Collatz<my_type>::fg_to_oe(root_full_fg_pattern, root->get_hwm_index(), false);
    //         logger->debug(
    //             "{} doesn't have a HWM ancestor and needs help:  Hits HWM at step {}.  Must match OE Chain: {}.  Full F-G Chain is: {}."
    //             , root_preamble
    //             , root->get_hwm_index() + 1
    //             , required_oe
    //             , root_full_fg_pattern
    //         );

    //         // Loop through lineage until we find the right child.
    //         bool found = false;
    //         while (! found) {
    //             descendant = lineage.back();
    //             descendant->spawn_children();
    //             for (uint8_t i = 0; i < descendant->get_child_count(); i++) {
    //                 // If the child matches the parent's next step, it is the correct child.
    //                 child = descendant->get_child_unsafe(i);
    //                 std::string child_fg_chain = child->get_fg_chain_string();
    //                 logger->debug("Trying child {} with value {} and FG chain: {}", i, child->get_value(), child_fg_chain);
    //                 if (root_full_fg_pattern.starts_with(child_fg_chain)) {
    //                     logger->debug("Child {} matches and joins the lineage.", child->get_value());
    //                     lineage.push_back(child);
    //                     if (child->is_below_high_water_mark()) {
    //                         logger->info(
    //                             "{} is covered by a descendant at {} (level {}) matching F-G chain {} ({}/{}, +{})."
    //                             , root_preamble
    //                             , child->get_value()
    //                             , child->get_level()
    //                             , child_fg_chain
    //                             , child->get_threes_value()
    //                             , child->get_twos_value()
    //                             , child->get_fg_constant()
    //                         );
    //                         size_t chain_width = lineage.back()->get_fg_chain_length();
    //                         size_t numeric_width = 0;
    //                         if constexpr(std::integral<my_type>) {
    //                             numeric_width = std::to_string(lineage.back()->get_value()).length();
    //                         } else {
    //                             numeric_width = lineage.back()->get_value().get_str().length();
    //                         }
    //                         for (const Node<my_type>* ancestor : lineage) {
    //                             std::string direction = "";
    //                             if (ancestor != root) {
    //                                 direction = ancestor->get_parent()->get_child(0) == ancestor ? "L" : "R";
    //                             }
    //                             logger->info(
    //                                 "  -> {:<{}}  {:>{}}  {}"
    //                                 , ancestor->get_fg_chain_string()
    //                                 , chain_width
    //                                 , ancestor->get_value()
    //                                 , numeric_width
    //                                 , direction
    //                             );
    //                         }
    //                         found = true;
    //                     }
    //                     break;
    //                 }
    //             }
    //         }
    //         // Clean up the root object, which will cascade.
    //         delete root;
    //     }
    // }

    // All done.
    std::cerr << "We need to make prune_level a separate option." << std::endl;
    return 0;
}
