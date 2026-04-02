#include <gmpxx.h>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include "collatz/binary_tree.hpp"
#include <CLI.hpp>
#include <unistd.h>
#include "collatz/binary_tree_coverage.hpp"
#include "collatz/concepts.hpp"
#include "collatz/logging.hpp"


//
// Dump a simple class here for ease-of-use to report and not muck up the BinaryTree class.
//
template<AnySupportedIntegral T>
class CoverageBuilder {
    private:
    BinaryTree<T> _tree;
    bool _use_precomputed = false;
    bool _is_verifying_non_hwm_nodes = false;

    public:
    CoverageBuilder(bool preserve_ancestors, BinaryTreeType tree_type, bool verify_non_hwm_nodes) {
        BinaryTreeOptions opts;
        opts.tree_type = tree_type;
        opts.prune_hwm_nodes = true;
        opts.preserve_ancestors = preserve_ancestors;
        opts.prune_parent_levels = true;
        opts.verify_non_hwm_nodes = verify_non_hwm_nodes;
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
void run(size_t levels, bool use_precomputed, bool show_ancestors, BinaryTreeType tree_type, size_t sleep_seconds, bool verify_non_hwm_nodes) {
    std::unordered_map<size_t, BinaryTreeCoverage<T>> coverage_map;
    CoverageBuilder<T> builder(show_ancestors, tree_type, verify_non_hwm_nodes);
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
            if constexpr(BuiltinIntegral<T>) {
                s_val = to_string_any(ancestor->get_value());
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
    // Coherency test.
    coverage_map = builder.get_tree().get_coverage_map();
    size_t failing_levels = 0;
    for (auto& [level, coverage] : coverage_map) {
        logger->debug("Testing level {} for coherency.", level);
        if (level > BinaryTreeCoverageConstants::MAX_KNOWN_COVERAGE_LEVEL) {
            logger->debug("Max known coverage reached for coherency testing.  Breaking out.");
            break;
        }
        if (builder.get_tree().get_coverage_map().at(level).get_covered() != BinaryTreeCoverageConstants::get_known_coverage<T>(level)) {
            logger->warn("Level {} failed.  Have {}, expected {}.", level, builder.get_tree().get_coverage_map().at(level).get_covered(), BinaryTreeCoverageConstants::get_known_coverage<T>(level));
            failing_levels++;
        }
    }
    if (failing_levels == 0) {
        logger->info("Coherency passed.  Computed coverage matches known values.");
    } else {
        logger->warn("Coherency failed!  {} levels failed.  See above.", failing_levels);
    }

    if (sleep_seconds > 0) {
        logger->info("Sleep requested for {} seconds.  Doing so.", sleep_seconds);
        sleep(sleep_seconds);
        logger->info("Sleep timer up.  Resuming.");
    }
}


int main(int argc, char **argv) {
    // Get logger.
    init_logger();
    // Process options.
    size_t levels;
    bool verbose = false;
    bool force_128bit = false;
    bool force_mpz = false;
    bool use_precomputed = false;
    bool show_ancestors = false;
    bool use_materialized_tree = false;
    bool verify_non_hwm_nodes = false;
    size_t sleep_seconds = 0;
    CLI::App options("Builds a BinaryTree and calculates the per-level and global coverage along the way.");
    options.add_flag("-a,--ancestors", show_ancestors, "Show a list of all the high-water mark ancestors when done.");
    options.add_flag("-i,--int128", force_128bit, "Use 128-bit integer instead of native 64-bit integral type.");
    options.add_option("-l,--levels", levels, "Number of levels to build the tree.")->default_val(16);
    options.add_flag("-m,--mpz", force_mpz, "Use GMP's mpz_class instead of native 64-bit integral type.");
    options.add_flag("-p,--precomputed", use_precomputed, "Use the precomputed table in BinaryTreeCoverage when possible.");
    options.add_option("-s,--sleep", sleep_seconds, "Sleep at program end (for debugging, mostly).  Zero disables.")->default_val(0);
    options.add_flag("-M,--materialize", use_materialized_tree, "Build a materialized tree instead of an implicit.  Why...?");
    options.add_flag(
        "-v,--verbose"
        , [&](size_t x){if(x>0) {verbose=true; logger->set_level(spdlog::level::debug);}}
        , "Enable verbosity."
    );
    options.add_flag("-V,--verify-non-hwm-nodes", verify_non_hwm_nodes, "Verify the non-HWM nodes as the tree builds.");
    CLI11_PARSE(options, argc, argv);
    logger->debug("Selected options were:");
    logger->debug("  Force MPZ: {}", force_mpz);
    logger->debug("  Force int128: {}", force_128bit);
    logger->debug("  Levels: {}", levels);
    logger->debug("  Use Precomputed: {}", use_precomputed);
    logger->debug("  Verbose: {}", verbose);
    logger->debug("  Verify Non-HWM Nodes: {}", verify_non_hwm_nodes);
    if (force_128bit && force_mpz) {
        throw(std::logic_error("You can't specify both 128-bit (-i) int and MPZ (-m)."));
    }

    // Build the tree object with no levels to start.
    std::string data_type = "uint64_t";
    if (force_128bit) {
        data_type = "uint128_t";
    }
    if (force_mpz) {
        data_type = "mpz_class";
    }
    // Now make sure it'll actually fit without overflowing.
    if (data_type == "uint64_t") {
        size_t max_level = BinaryTreeMath<uint64_t>::st_max_level_of_type();
        if (levels > max_level) {
            logger->info("Level (-l) is over {} and you didn't specify -i or -m.  Auto upgrading from uint64_t to uint128_t.", max_level);
            data_type = "uint128_t";
        }
    }
    if (data_type == "uint128_t") {
        size_t max_level = BinaryTreeMath<uint128_t>::st_max_level_of_type();
        if (levels > max_level) {
            logger->info("Level (-l) is over {} and you didn't specify -m.  Auto upgrading from uint128_t to mpz_class.", max_level);
            data_type = "mpz_class";
        }
    }
    BinaryTreeType tree_type = use_materialized_tree ? BinaryTreeType::MATERIALIZED : BinaryTreeType::IMPLICIT;
    logger->info("Building tree with {} levels, using {}, tree type is {}, verifying non-HWM nodes is: {}."
        , levels
        , data_type
        , use_materialized_tree ? "Materialized" : "Implicit"
        , verify_non_hwm_nodes
    );
    if (use_precomputed) {
        logger->warn("You requested a precomputed table.  These are statically looked up, not computed!");
    }
    if (data_type == "uint64_t") {
        run<uint64_t>(levels, use_precomputed, show_ancestors, tree_type, sleep_seconds, verify_non_hwm_nodes);
    }
    if (data_type == "uint128_t") {
        run<uint128_t>(levels, use_precomputed, show_ancestors, tree_type, sleep_seconds, verify_non_hwm_nodes);
    }
    if (data_type == "mpz_class") {
        run<mpz_class>(levels, use_precomputed, show_ancestors, tree_type, sleep_seconds, verify_non_hwm_nodes);
    }

    return 0;
}
