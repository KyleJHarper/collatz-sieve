#include <gmpxx.h>
#include <stdint.h>
#include "collatz/binary_tree.hpp"
#include "collatz/concepts.hpp"
#include "CLI11.hpp"
#include "logging.hpp"


//
// Dump a simple class here for ease-of-use to report and not muck up the BinaryTree class.
//
template<IntegralOrMPZClass T>
class CoverageBuilder {
    private:
    BinaryTree<T> _tree;

    public:
    CoverageBuilder() {
        _tree.init(0, false);
    }

    const BinaryTree<T>& get_tree() const { return _tree; }
    // Add levels until we reach `levels` from caller.
    void run(size_t levels) {
        while(_tree.get_max_level() < levels) {
            add_level();
        }
    }
    void add_level() {
        size_t next_level = _tree.get_max_level() + 1;
        logger->debug("Building level {}...", next_level);
        _tree.add_level();
        BinaryTreeCoverage<T> coverage = _tree.get_coverage_map().find(next_level)->second;
        logger->debug("Level {} coverage was: {:.4f}% ({}/{})", next_level, coverage.get_ratio(true).get_d(), coverage.get_covered(), coverage.get_total());
    }
};


template<IntegralOrMPZClass T>
void run(size_t levels) {
    std::unordered_map<size_t, BinaryTreeCoverage<T>> coverage_map;
    CoverageBuilder<T> builder;
    builder.run(levels);
    coverage_map = builder.get_tree().get_coverage_map();
    BinaryTreeCoverage<T> global_coverage;
    for (auto& [level, coverage] : coverage_map) {
        global_coverage.merge(coverage);
    }
    logger->info("Global Coverage: {:.4f}% ({}/{})", global_coverage.get_ratio(true).get_d(), global_coverage.get_covered(), global_coverage.get_total());
}


int main(int argc, char **argv) {
    // Get logger.
    init_logger();
    // Process options.
    size_t levels;
    bool verbose;
    bool force_mpz;
    CLI::App options("Builds a BinaryTree and calculates the per-level and global coverage along the way.");
    options.add_option("-l,--levels", levels, "Number of levels to build the tree.")->default_val(16);
    options.add_flag("-m,--mpz", force_mpz, "Use GMP's mpz_class instead of native 64-bit integral type.");
    options.add_flag(
        "-v,--verbose"
        , [&](size_t x){if(x>0) {verbose=true; logger->set_level(spdlog::level::debug);}}
        , "Enable verbosity."
    );
    CLI11_PARSE(options, argc, argv);
    logger->debug("Selected options were:");
    logger->debug("  Force MPZ: {}", force_mpz);
    logger->debug("  Levels: {}", levels);
    logger->debug("  Verbose: {}", verbose);

    // Build the tree object with no levels to start.
    bool use_mpz = (force_mpz || levels > 63) ? true : false;
    logger->info("Building tree with {} levels, using {}.", levels, use_mpz ? "GMP" : "uint64_t");
    if (use_mpz) {
        run<mpz_class>(levels);
    } else {
        run<uint64_t>(levels);
    }

    return 0;
}
