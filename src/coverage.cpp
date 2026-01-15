#include <gmpxx.h>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include "collatz/binary_tree.hpp"
#include <CLI.hpp>
#include "collatz/logging.hpp"


//
// Dump a simple class here for ease-of-use to report and not muck up the BinaryTree class.
//
template<AnySupportedIntegral T>
class CoverageBuilder {
    private:
    BinaryTree<T> _tree;
    bool _use_precomputed = false;

    public:
    CoverageBuilder(bool preserve_ancestors) {
        BinaryTreeOptions opts;
        opts.track_node_metadata = false;
        opts.prune_hwm_nodes = true;
        opts.preserve_ancestors = preserve_ancestors;
        opts.prune_parent_levels = true;
        _tree.init(0, opts);
    }

    const BinaryTree<T>& get_tree() const { return _tree; }
    // Add levels until we reach `levels` from caller.
    void run(size_t levels) {
        if (_use_precomputed) {
            if (levels > BinaryTreeCoverageConstants::MAX_KNOWN_COVERAGE_LEVEL) {
                std::string msg = "You can't use precomputed coverage after level ";
                msg += std::to_string(BinaryTreeCoverageConstants::MAX_KNOWN_COVERAGE_LEVEL) + ".";
                msg += "  (You requested " + std::to_string(levels) + " levels.)";
                throw std::out_of_range(msg);
            }
            // Just spit out the results.
            BinaryTreeCoverage<T> global_coverage;
            for (size_t level = 1; level <= levels; level++) {
                BinaryTreeCoverage<T> coverage;
                coverage.set_covered(BinaryTreeCoverageConstants::get_known_coverage<T>(level));
                coverage.set_total(BinaryTreeCoverageConstants::get_total<T>(level));
                global_coverage.merge(coverage);
                logger->debug("Level {} coverage was: {:.4f}% ({}/{} | {} uncovered globally)", level, coverage.get_ratio(true).get_d(), coverage.get_covered(), coverage.get_total(), global_coverage.get_uncovered());
            }
        } else {
            while(_tree.get_level_count() < levels) {
                add_level();
            }
        }
    }
    void add_level() {
        size_t next_level = _tree.get_level_count() + 1;
        logger->debug("Building level {}...", next_level);
        _tree.add_level();
        BinaryTreeCoverage<T> coverage = _tree.get_coverage_map().find(next_level)->second;
        BinaryTreeCoverage<T> global_coverage;
        for (auto& [level, coverage] : _tree.get_coverage_map()) {
            global_coverage.merge(_tree.get_coverage_map().find(level)->second);
        }
        logger->debug("Level {} coverage was: {:.4f}% ({}/{} | {} uncovered globally)", next_level, coverage.get_ratio(true).get_d(), coverage.get_covered(), coverage.get_total(), global_coverage.get_uncovered());
    }
    void use_precomputed(bool value) { _use_precomputed = value; }
};


template<AnySupportedIntegral T>
void run(size_t levels, bool use_precomputed, bool show_ancestors) {
    std::unordered_map<size_t, BinaryTreeCoverage<T>> coverage_map;
    CoverageBuilder<T> builder(show_ancestors);
    builder.get_tree().assert_level_will_fit(levels);
    builder.use_precomputed(use_precomputed);
    builder.run(levels);
    BinaryTreeCoverage<T> global_coverage;
    if (use_precomputed) {
        for (size_t level = 1; level <= levels; level++) {
            BinaryTreeCoverage<T> coverage;
            coverage.set_covered(BinaryTreeCoverageConstants::get_known_coverage<T>(level));
            coverage.set_total(BinaryTreeCoverageConstants::get_total<T>(level));
            global_coverage.merge(coverage);
        }
    } else {
        coverage_map = builder.get_tree().get_coverage_map();
        for (auto& [level, coverage] : coverage_map) {
            global_coverage.merge(coverage);
        }
    }
    logger->info("Global Coverage: {:.4f}% ({}/{} | {} uncovered)", global_coverage.get_ratio(true).get_d(), global_coverage.get_covered(), global_coverage.get_total(), global_coverage.get_uncovered());
    if (show_ancestors) {
        std::string merged = "";
        for (Node<T>* ancestor : builder.get_tree().get_ancestors()) {
            std::string s_val;
            if constexpr(std::integral<T>) {
                s_val = std::to_string(ancestor->get_value());
            } else {
                s_val = ancestor->get_value().get_str();
            }
            if (merged.empty()) {
                merged = s_val;
            } else {
                merged += ", " + s_val;
            }
        }
        logger->info("Ancestors: " + merged);
    }
}


int main(int argc, char **argv) {
    // Get logger.
    init_logger();
    // Process options.
    size_t levels;
    bool verbose = false;
    bool force_mpz = false;
    bool use_precomputed = false;
    bool show_ancestors = false;
    CLI::App options("Builds a BinaryTree and calculates the per-level and global coverage along the way.");
    options.add_flag("-a,--ancestors", show_ancestors, "Show a list of all the high-water mark ancestors when done.");
    options.add_option("-l,--levels", levels, "Number of levels to build the tree.")->default_val(16);
    options.add_flag("-m,--mpz", force_mpz, "Use GMP's mpz_class instead of native 64-bit integral type.");
    options.add_flag("-p,--precomputed", use_precomputed, "Use the precomputed table in BinaryTreeCoverage when possible.");
    options.add_flag(
        "-v,--verbose"
        , [&](size_t x){if(x>0) {verbose=true; logger->set_level(spdlog::level::debug);}}
        , "Enable verbosity."
    );
    CLI11_PARSE(options, argc, argv);
    logger->debug("Selected options were:");
    logger->debug("  Force MPZ: {}", force_mpz);
    logger->debug("  Levels: {}", levels);
    logger->debug("  Use Precomputed: {}", use_precomputed);
    logger->debug("  Verbose: {}", verbose);

    // Build the tree object with no levels to start.
    bool use_mpz = (force_mpz || levels > 63) ? true : false;
    logger->info("Building tree with {} levels, using {}.", levels, use_mpz ? "GMP" : "uint64_t");
    if (use_precomputed) {
        logger->warn("You requested a precomputed table.  These are statically looked up, not computed!");
    }
    if (use_mpz) {
        run<mpz_class>(levels, use_precomputed, show_ancestors);
    } else {
        run<uint64_t>(levels, use_precomputed, show_ancestors);
    }

    return 0;
}
