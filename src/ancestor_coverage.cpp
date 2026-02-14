#include <gmpxx.h>
#include <stdint.h>
#include <string>
#include "collatz/binary_tree.hpp"
#include "collatz/concepts.hpp"
#include "collatz/node.hpp"
#include "collatz/collatz.hpp"
#include <CLI.hpp>
#include "collatz/logging.hpp"


template<AnySupportedIntegral T>
class AncestorResult {
    // Just make it all public.  Could use a struct, but whatever.
    public:
    std::vector<Node<T>*> lineage;
};


template<AnySupportedIntegral T>
std::vector<AncestorResult<T>> run (size_t levels) {
    // Build the tree.
    logger->info("Building tree with {} levels.", levels);
    BinaryTreeOptions opts;
    opts.preserve_ancestors = true;
    opts.prune_hwm_nodes = true;
    opts.prune_parent_levels = false;
    BinaryTree<T> tree(levels, opts);

    // Scan.
    // We don't need to build a whole tree and traverse it.  That'll just use a stupid amount of memory.
    // Instead, build children on-the-fly.
    std::vector<AncestorResult<T>> results;
    // std::vector<Node<T>*> lineage;
    for (size_t level = 1; level <= tree.get_level_count(); level++) {
        logger->debug("Scanning level {}", level);
        for (const Node<T>* node : tree.get_level_map().at(level)) {
            if (node->is_below_high_water_mark() || node->has_high_water_mark_ancestor()) {
                if constexpr(std::integral<T>) {
                    throw std::logic_error("Node " + std::to_string(node->get_value()) + " somehow survived purning but hit HWM or has ancestor...");
                } else {
                    throw std::logic_error("Node " + node->get_value().get_str() + " somehow survived purning but hit HWM or has ancestor...");
                }
            }

            // Make a result object.
            AncestorResult<T> result;

            // Make a copy of the node so we don't break the tree.
            Node<T>* root = new Node<T>(node->get_value());
            result.lineage.push_back(root);
            Node<T>* descendant;
            Node<T>* child;
            std::string root_preamble = "";
            if constexpr(std::integral<T>) {
                root_preamble = "Node " + std::to_string(root->get_value()) + " (level " + std::to_string(level) + ")";
            } else {
                root_preamble = "Node " + root->get_value().get_str() + " (level " + std::to_string(level) + ")";
            }
            std::string root_full_fg_pattern = Collatz<T>::st_get_fg_chain_string(root->get_value());
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
                descendant = result.lineage.back();
                descendant->spawn_children();
                for (uint8_t i = 0; i < descendant->get_child_count(); i++) {
                    // If the child matches the parent's next step, it is the correct child.
                    child = descendant->get_child_unsafe(i);
                    std::string child_fg_chain = child->get_fg_chain_string();
                    logger->debug("Trying child {} with value {} and FG chain: {}", i, child->get_value(), child_fg_chain);
                    if (root_full_fg_pattern.starts_with(child_fg_chain)) {
                        logger->debug("Child {} matches and joins the lineage.", child->get_value());
                        result.lineage.push_back(child);
                        if (child->is_below_high_water_mark()) {
                            results.push_back(result);
                            found = true;
                        }
                        break;
                    }
                }
            }
        }
    }
    return results;
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

    // Run it.
    auto results = run<my_type>(levels);

    // Report
    for (auto& result : results) {
        Node<my_type>* root = result.lineage.front();
        Node<my_type>* resolver = result.lineage.back();
        size_t level = root->get_level();
        my_type start = root->get_value();
        my_type resolver_value = resolver->get_value();
        size_t steps = result.lineage.size() - 1;
        std::string direction = "";
        for (Node<my_type>* descendant : result.lineage) {
            if (descendant != root) {
                direction += descendant->get_parent()->get_child(0)->get_value() == descendant->get_value() ? "L" : "R";
            }
        }
        logger->info("Level {} | Node {} | Steps: {} | Resolver: {} | Direction: {}", level, start, steps, resolver_value, direction);

    }


    // All done.
    return 0;
}
