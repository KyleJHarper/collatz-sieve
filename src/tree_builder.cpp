#include <CLI/CLI.hpp>
#include "collatz/concepts.hpp"
#include "collatz/logging.hpp"
#include "collatz/binary_tree.hpp"
#include "collatz/typedefs.hpp"
#include <filesystem>



struct LaunchContext {
    level_t start_level = 0;
    level_t end_level = 0;
    std::string existing_tree_path = "";
    std::string data_dir = "data";
    bool exclude_value_map = false;
    int compression_level = 22;
    bool overwrite = false;
    bool skip_verify = false;
};




template<AnySupportedIntegral T>
void do_it(LaunchContext& ctx) {
    // Calculate the data type string segement.
    std::string data_type_segment = "64bit";
    if constexpr(Int128Integral<T>) {
        data_type_segment = "128bit";
    } else if constexpr(GMPIntegral<T>) {
        data_type_segment = "mpz";
    }

    // Calculate the value-map segment.
    std::string value_map_segment = ctx.exclude_value_map ? "evm" : "ivm";

    // Make a bare binary tree object.
    BinaryTree<T> tree;

    // If the end level is lower than the start, set them equal (single-level mode).
    if (ctx.end_level < ctx.start_level) {
        ctx.end_level = ctx.start_level;
    }

    // Load the existing tree file if requested.
    if (ctx.existing_tree_path.empty() == false) {
        logger->info("Loading existing tree from path: {}", ctx.existing_tree_path);
        tree.load(ctx.existing_tree_path);

        // If the tree level exceeds the end, there's no work to do.
        if (tree.get_level_count() >= ctx.end_level) {
            logger->warn("The supplied tree has {} levels, but you requested an end level of {}.  There's no work to do.", tree.get_level_count(), ctx.end_level);
            return;
        }

        // If the tree level exceeds the start, emit a warning.
        if (tree.get_level_count() > ctx.start_level) {
            logger->warn("The supplied tree has {} levels, but you requested exporting from level {}.  Level(s) will be skipped.", tree.get_level_count(), ctx.start_level);
        }
    }

    // Bring the tree to the required level minus 1.
    if (tree.get_level_count() < (ctx.start_level - 1)) {
        logger->info("Increasing tree level from {} to {} to get started.", tree.get_level_count(), ctx.start_level);
        while (tree.get_level_count() < (ctx.start_level - 1)) {
            level_t next_level = tree.get_level_count() + 1;
            logger->debug("Building tree level {}.", next_level);
            tree.add_level();
            logger->debug("Level {} built.", next_level);
        }
    }

    // Loop until the end level, exporting along the way.
    while (tree.get_level_count() < ctx.end_level) {
        // Announce level.
        logger->info("Processing tree level: {}", tree.get_level_count() + 1);

        // Add the next level.
        logger->info("  Growing tree.");
        tree.add_level();

        // Generate the value map, if requested.
        if (ctx.exclude_value_map == false) {
            logger->info("  Building value map.");
            tree.generate_value_map();
        }

        // Export the tree to disk.
        logger->info("  Exporting tree to disk.");
        std::string filename = std::format(
            "implicit_l{:02}_{}_{}.htree{}"
            , tree.get_level_count()
            , data_type_segment
            , value_map_segment
            , ctx.compression_level == 0 ? "" : ".zstd"
        );
        std::string file_path = ctx.data_dir + "/" + filename;
        if (std::filesystem::exists(file_path)) {
            if (ctx.overwrite == false) {
                logger->error("  File already exists: {}.  Refusing to overwrite.", file_path);
                return;
            }
            logger->warn("  File already exists: {}.  Overwrite (-o) is enabled, so we're clobbering it.", file_path);
        }
        tree.save(file_path, ctx.compression_level);

        // Verify the on-disk version with our in-memory version.
        if (ctx.skip_verify) {
            logger->warn("  Verification skipped (-s) by user.");
        } else {
            logger->info("  Verifying on-disk tree with in-memory representation.");
            std::string err;
            BinaryTree<T> on_disk_tree;
            on_disk_tree.load(file_path);
            if (tree.equal(on_disk_tree, &err) == false) {
                logger->error("  Trees do not match!  Error output follows:");
                logger->error(err);
                logger->error("  Quitting for safety.");
                return;
            }
        }
    }
    logger->info("Done building levels.");
}



int main(int argc, char **argv) {
    // Initialize the logger.
    init_logger("log/tree_builder.log");



    // Process options.
    LaunchContext ctx;
    bool verbose = false;
    bool use_mpz = false;
    bool use_i128 = false;

    CLI::App options("Build one or more trees and save them to disk for others to import later.  Always Implicit-style tree.");
    options.add_option("-c,--compression-level", ctx.compression_level, "Compression level to use.  Default is: 22.");
    options.add_option("-d,--data-dir", ctx.data_dir, "The relative directory to write files to.  Default is: data");
    options.add_option("-e,--end-level", ctx.end_level, "The last level to build.  If 0, ignored, and a single level is built and exported.");
    options.add_flag("-i,--int128", use_i128, "Build 128-bit trees (default is uint64_t).");
    options.add_option("-l,--level", ctx.start_level, "The first level to build.  See -e.");
    options.add_flag("-m,--mpz", use_mpz, "Build GMP (mpz_class) trees (default is uint64_t).");
    options.add_flag("-o,--overwrite", ctx.overwrite, "Overwrite existing files without warning (clobber them).");
    options.add_flag("-s,--skip-verify", ctx.skip_verify, "Skips verification of trees.  Probably a bad idea, but cuts RAM usage in half.");
    options.add_option("-t,--tree", ctx.existing_tree_path, "Path to an existing tree export to start with instead of building from nothing.");
    options.add_flag(
        "-v,--verbose"
        , [&](size_t x){if(x>0) {verbose=true; logger->set_level(spdlog::level::debug);}}
        , "Enable verbosity."
    );
    options.add_flag("-x,--exclude-value-map", ctx.exclude_value_map, "Don't generate the value map.  Saves space, but cannot be used by verifier.");
    CLI11_PARSE(options, argc, argv);
    logger->debug("Selected options were:");
    logger->debug("  Compression Level  (-c): {}", ctx.compression_level);
    logger->debug("  Clobber Files      (-C): {}", ctx.overwrite);
    logger->debug("  Data Dir           (-d): {}", ctx.data_dir);
    logger->debug("  End Level          (-e): {}", ctx.end_level);
    logger->debug("  Use int128         (-i): {}", use_i128);
    logger->debug("  Level              (-l): {}", ctx.start_level);
    logger->debug("  Use MPZ            (-m): {}", use_mpz);
    logger->debug("  Skip Verify        (-s): {}", ctx.skip_verify);
    logger->debug("  Existing Tree Path (-t): {}", ctx.existing_tree_path);
    logger->debug("  Verbose            (-v): {}", verbose);
    logger->debug("  Exclude Value Map  (-x): {}", ctx.exclude_value_map);



    // Preflight checks.
    // Start level must be greater than zero.
    if (ctx.start_level < 1) {
        logger->error("The level to start (-l) must be greater than zero.");
        return 1;
    }

    // End level needs to be >= starting level if it's non-zero.
    if (ctx.end_level > 0 && ctx.end_level < ctx.start_level) {
        logger->error("You must specify an end level (-e) that is greater than or equal to the start level.  You can leave it unset (or 0) to just build one tree output.");
        return 1;
    }

    // Compression level must be between 0 and 22.
    if (ctx.compression_level < 0 || ctx.compression_level > 22) {
        logger->error("Compression level (-c) must be between 0 (disbled) and 22 (max).  You sent {}.", ctx.compression_level);
        return 1;
    }

    // Existing tree path must exist.
    if (ctx.existing_tree_path.empty() == false && std::filesystem::exists(ctx.existing_tree_path) == false) {
        logger->error("Cannot find the supplied path for an existing tree file (-t): {}", ctx.existing_tree_path);
        return 1;
    }

    // Can only specify zero or one data type.
    if (use_i128 && use_mpz) {
        logger->error("You cannot specify both uint128_t (-i) and mpz_class (-m) at the same time.");
        return 1;
    }

    // Data directory has to exist.
    if (std::filesystem::is_directory(ctx.data_dir) == false) {
        logger->error("Data directory '{}' doesn't seem to exist and/or be a directory.", ctx.data_dir);
        return 1;
    }



    // Launch with the correct type.
    if (use_i128) {
        do_it<uint128_t>(ctx);
    } else if (use_mpz) {
        do_it<mpz_class>(ctx);
    } else {
        do_it<uint64_t>(ctx);
    }



    // All done.
    logger->info("All done.");
    return 0;
}
