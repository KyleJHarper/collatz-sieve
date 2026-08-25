#include <CLI/CLI.hpp>
#include <filesystem>
#include <gmp.h>
#include "collatz/abi.hpp"
#include "collatz/concepts.hpp"
#include "collatz/gpu.hpp"
#include "collatz/int128.hpp"
#include "collatz/logging.hpp"
#include "collatz/binary_tree.hpp"
#include "collatz/verifier_gpu.hpp"
#include "collatz/verifier_cpu.hpp"
#include "collatz/progress_report.hpp"
#include <csignal>
#include <atomic>




// Setup signal handlines.
std::atomic<bool> global_shutdown_atomic = false;
void shutdown_signal_handler(int signum) {
    // Sig num is required even though not used.  Use it to get rid of compiler warnings.
    if (signum < 0) {
        std::cerr << "How did signum go negative...?" << std::endl;
    }
    if (global_shutdown_atomic.load() == true) {
        logger->critical("Second interrupt received.  Aborting uncleanly.");
        logger->flush();
        std::abort();
    }
    global_shutdown_atomic.store(true, std::memory_order_relaxed);
}



struct LaunchContext {
    std::string tree_path = "";
    bool use_cpu = false;
    bool use_gpu = false;
    bool detailed_metrics = false;
    bool no_iv_table = false;
    std::string ilp_path = "";
    std::string start_s = "";
    std::string end_s = "";
    uint32_t update_ms = 1000;
    size_t gpu_memory_limit = 0;
    size_t gpu_scales_per_run = 100;
};



template<AnySupportedIntegral T>
std::string inline generate_report(Verifier<T>& verifier, const LaunchContext& ctx) {
    // Build a blank string to begin with.
    std::string result = "";

    // Add two blank lines.
    result += "\n";

    // Create a convenient link to the tree.
    const BinaryTree<T>& tree = verifier.get_tree();

    // Fixed properties.  Only need the size of the largest one for spacing.
    const size_t fp_width = 9;
    // Tree needs level, size, and survivor count.
    result += std::format(
        "\n{:<{}}: Level {} | {} MB | {} Survivors"
        , "Tree"
        , fp_width
        , tree.get_level_count()
        , to_string_any(tree.deep_size() / 1024 / 1024, true)
        , to_string_any(tree.get_uncovered_values().cardinality(), true)
    );
    // Data type
    result += std::format(
        "\n{:<{}}: {} | {} bits"
        , "Data Type"
        , fp_width
        , ABI::demangle<T>()
        , GMPIntegral<T> ? "unlimited" : to_string_any(sizeof(T) * 8)
    );
    // Coverage and residue is from the tree.
    double coverage = tree.get_coverage_map().at(tree.get_level_count()).get_ratio(true).get_d();
    double residue = 100 - coverage;
    result += std::format("\n{:<{}}: {:.4f}% | {:.4f}% Residue", "Coverage", fp_width, coverage, residue);
    // Verifier includes the detailed metrics flag and max IV table setting.
    result += std::format(
        "\n{:<{}}: Detailed Metrics {} | Max IV Table {}"
        , "Verifier"
        , fp_width
        , ctx.detailed_metrics ? "Enabled" : "Disabled"
        , ctx.no_iv_table ? "Disabled" : "Enabled"
    );
    // CPU threads is simply the OMP value.
    result += std::format("\n{:<{}}: {} Threads", "CPU", fp_width, omp_get_max_threads());
    // GPU Memory is only available on the GPU verifier, which is only allowed to use fixed-width types.
    if (auto* cpu = dynamic_cast<CPUVerifier<T>*>(&verifier)) {
        result += std::format("\n{:<{}}: n/a", "GPU", fp_width);
    } else {
        // Wrap in a FixedWidth constexpr to avoid errors since GMP isn't allowed on GPU.
        if constexpr(FixedWidthIntegral<T>) {
            auto* gpu = dynamic_cast<GPUVerifier<T>*>(&verifier);
            result += std::format(
                "\n{:<{}}: {} bytes VRAM | {} Scales Per Run"
                , "GPU"
                , fp_width
                , to_string_any(gpu->get_gpu_buffer_used(), true)
                , gpu->get_scales_per_run()
            );
        }
    }

    // Add a blank line before metrics.
    result += "\n";

    // Get a link to the metrics now.
    const VerifierMetric& metrics = verifier.get_metrics();

    // Determine the widest value from "effective values", or use "steps" if detailed metrics is on.
    size_t total_width = to_string_any(metrics.effective_nodes_verified(), true).size();
    size_t per_s_width = to_string_any(metrics.effective_nodes_per_ms() * 1000, true).size();
    if (auto* cpu = dynamic_cast<CPUVerifier<T>*>(&verifier)) {
        if (cpu->is_detailed_metrics_enabled() == true) {
            total_width = to_string_any(metrics.steps_total_atomic.load(), true).size();
            per_s_width = to_string_any(metrics.steps_per_ms() * 1000, true).size();
        }
    }
    // Make sure they're at least 10 to cover the headers.
    if (total_width < 10) { total_width = 10; }
    if (per_s_width < 10) { per_s_width = 10; }
    // Add three spaces for the columns.
    total_width += 3;
    per_s_width += 3;

    // Set the metric_width to 23 for "Steps Skipped By Affine".
    const size_t metric_width = 23;

    // Add the header.
    result += std::format("\n{:<{}}", "", metric_width);
    result += std::format("{:>{}}", "Total", total_width);
    result += std::format("{:>{}}", "Per/s", per_s_width);

    // Surviving Values
    result += std::format("\n{:<{}}", "Surviving Values", metric_width);
    result += std::format("{:>{}}", to_string_any(metrics.nodes_verified_atomic.load(), true), total_width);
    result += std::format("{:>{}}", to_string_any(metrics.nodes_per_s(), true), per_s_width);

    // Effective Values
    result += std::format("\n{:<{}}", "Effective Values", metric_width);
    result += std::format("{:>{}}", to_string_any(metrics.effective_nodes_verified(), true), total_width);
    result += std::format("{:>{}}", to_string_any(metrics.effective_nodes_per_s(), true), per_s_width);

    // Steps
    result += std::format("\n{:<{}}", "Steps", metric_width);
    result += std::format("{:>{}}", to_string_any(metrics.steps_total_atomic.load(), true), total_width);
    result += std::format("{:>{}}", to_string_any(metrics.steps_per_s(), true), per_s_width);

    // Steps Skippable By Affine
    result += std::format("\n{:<{}}", "Steps Skipped By Affine", metric_width);
    result += std::format("{:>{}}", to_string_any(metrics.steps_skippable_by_affine_stride_atomic.load(), true), total_width);
    result += std::format("{:>{}}", to_string_any(metrics.steps_skippable_by_affine_stride_per_s(), true), per_s_width);
    result += std::format("  {:<.2f}%", 100 * metrics.skip_rate_of_affine_stride());

    // Steps Skippable By AST
    result += std::format("\n{:<{}}", "Steps Skipped By AST", metric_width);
    result += std::format("{:>{}}", to_string_any(metrics.steps_skippable_by_ast_atomic.load(), true), total_width);
    result += std::format("{:>{}}", to_string_any(metrics.steps_skippable_by_ast_per_s(), true), per_s_width);
    result += std::format("  {:<.2f}%", 100 * metrics.skip_rate_of_ast());

    // GPU Kernel Launches
    result += std::format("\n{:<{}}", "GPU Kernel Launches", metric_width);
    result += std::format("{:>{}}", to_string_any(metrics.gpu_kernel_launches_atomic.load(), true), total_width);
    result += std::format("{:>{}}", to_string_any(metrics.gpu_kernel_launches_per_s(), true), per_s_width);

    // GPU Kernel Launches
    result += std::format("\n{:<{}}", "GPU Overflows", metric_width);
    result += std::format("{:>{}}", to_string_any(metrics.gpu_overflows_processed_atomic.load(), true), total_width);
    result += std::format("{:>{}}", to_string_any(metrics.gpu_overflows_per_s(), true), per_s_width);

    // Timer and Last Value Approximation.
    size_t duration_seconds = metrics.duration_ms.count() / 1000;
    size_t days = duration_seconds / 86400;
    size_t hours = (duration_seconds % 86400) / 3600;
    size_t minutes = (duration_seconds % 3600) / 60;
    size_t seconds = duration_seconds % 60;
    // Build the approximation with 0 or whatever the user sent, parsed correctly.
    T approximation = 0;
    T start = 0;
    T end = 0;
    if constexpr(FixedWidthIntegral<T>) {
        if constexpr(sizeof(T) * 8 <= 64) {
            start = std::atoll(ctx.start_s.c_str());
            end = std::atoll(ctx.end_s.c_str());
        } else if constexpr(sizeof(T) * 8 == 128) {
            start = Int128::str_to_uint128(ctx.start_s);
            end = Int128::str_to_uint128(ctx.end_s);
        }
    } else if constexpr(GMPIntegral<T>) {
        start = mpz_class(ctx.start_s);
        end = mpz_class(ctx.end_s);
    }
    // If the start would result in a multiplier of 1, the true "start" will be the minimum uncovered value.
    if (start < tree.get_uncovered_values().maximum()) {
        start = tree.get_uncovered_values().minimum();
    }
    // Now that we know the approximate starting value, add the effective range covered to appoximate current value.
    approximation = start + metrics.effective_nodes_verified();
    // Use the start and end to calculate the total todo and a float of the progress.
    T total_todo = 0;
    mpf_class total_finished = 0;
    if (end > start) {
        total_todo = end - start;
        total_finished = (100.0 * approximation) / total_todo;
    }
    result += std::format(
        "\n\n{} days {:02}:{:02}:{:02} | Last Value: ~{} | Done: {:.2f}%"
        , days
        , hours
        , minutes
        , seconds
        , to_string_any(approximation, true)
        , total_finished.get_d()
    );

    // Return it.
    return result;
}



template<AnySupportedIntegral T>
int do_it(const LaunchContext& ctx) {
    // Load the tree.
    logger->info("Loading tree from file.");
    BinaryTree<T> tree;
    tree.load(ctx.tree_path);

    // Ensure the value map has been generated.
    if (tree.get_uncovered_values().empty()) {
        logger->error("Must generate the value map in the Binary Tree before using it for verification.");
        return 1;
    }

    // Cast strings into correct types.
    T start;
    T end;
    if constexpr(FixedWidthIntegral<T>) {
        if constexpr(sizeof(T) * 8 <= 64) {
            start = std::atoll(ctx.start_s.c_str());
            end = std::atoll(ctx.end_s.c_str());
        } else if constexpr(sizeof(T) * 8 == 128) {
            start = Int128::str_to_uint128(ctx.start_s);
            end = Int128::str_to_uint128(ctx.end_s);
        }
    } else if constexpr(GMPIntegral<T>) {
        start = mpz_class(ctx.start_s);
        end = mpz_class(ctx.end_s);
    }

    // Create a file handle for metric output, if requested.
    std::ofstream fh;
    if (ctx.ilp_path.empty() == false) {
        fh.open(ctx.ilp_path, std::ios::out | std::ios::app);
    }

    // Create a progress reporter for the screen.
    ProgressReport progress;

    // Verify
    if (ctx.use_cpu) {
        logger->info("Building CPU Verifier.");
        CPUVerifier<T> cpu_verifier(tree);
        logger->debug("Setting start value to {}.", start);
        cpu_verifier.set_start_value(start);
        logger->debug("Setting end value to {}.", end);
        cpu_verifier.set_end_value(end);
        if (ctx.detailed_metrics) {
            logger->debug("Enabling detailed metrics.");
            cpu_verifier.enable_detailed_metrics();
        } else {
            logger->debug("Disabling detailed metrics.");
            cpu_verifier.disable_detailed_metrics();
        }
        if (ctx.no_iv_table) {
            logger->debug("Disabling the Max IV table.");
            cpu_verifier.disable_max_iv_table();
        } else {
            logger->debug("Enabling the Max IV table.");
            cpu_verifier.enable_max_iv_table();
        }
        logger->info("Launching verifier.");
        cpu_verifier.start();
        while (cpu_verifier.get_state() != VerifierState::STOPPED) {
            std::this_thread::sleep_for(std::chrono::milliseconds(ctx.update_ms));
            // Write the latest report.
            progress.report(generate_report(cpu_verifier, ctx));
            // Report to ILP file, if necessary.
            if (fh.is_open()) {
                fh << cpu_verifier.get_metrics().emit_ilp() << std::endl;
            }
            if (global_shutdown_atomic.load() == true) {
                progress.clear();
                logger->warn("Shutdown requested.  Requesting verifier to stop.");
                cpu_verifier.stop();
            }
        }
        progress.clear();
        logger->info("Final Report:\n{}", generate_report(cpu_verifier, ctx));
    } else if (ctx.use_gpu) {
        if constexpr(FixedWidthIntegral<T>) {
            logger->info("Building GPU Verifier.");
            GPUVerifier<T> gpu_verifier(tree);
            logger->debug("Setting start value to {}.", start);
            gpu_verifier.set_start_value(start);
            logger->debug("Setting end value to {}.", end);
            gpu_verifier.set_end_value(end);
            if (ctx.no_iv_table) {
                logger->debug("Disabling the Max IV table.");
                gpu_verifier.disable_max_iv_table();
            } else {
                logger->debug("Enabling the Max IV table.");
                gpu_verifier.enable_max_iv_table();
            }
            logger->debug("Setting GPU memory limit to {} bytes.", ctx.gpu_memory_limit);
            gpu_verifier.set_gpu_buffer_limit(ctx.gpu_memory_limit);
            logger->debug("Setting scales per run to {}.", ctx.gpu_scales_per_run);
            gpu_verifier.set_scales_per_run(ctx.gpu_scales_per_run);
            logger->info("Launching verifier.");
            gpu_verifier.start();
            while (gpu_verifier.get_state() != VerifierState::STOPPED) {
                std::this_thread::sleep_for(std::chrono::milliseconds(ctx.update_ms));
                // Write the latest report.
                progress.report(generate_report(gpu_verifier, ctx));
                // Report to ILP file, if necessary.
                if (fh.is_open()) {
                    fh << gpu_verifier.get_metrics().emit_ilp() << std::endl;
                }
                if (global_shutdown_atomic.load() == true) {
                    progress.clear();
                    logger->warn("Shutdown requested.  Requesting verifier to stop.");
                    gpu_verifier.stop();
                }
            }
            progress.clear();
            logger->info("Final Report:\n{}", generate_report(gpu_verifier, ctx));
        } else {
            logger->error("Cannot use GMP with GPU verifier (-g).  Must use <=128 bit integer.");
            return 1;
        }
    } else {
        logger->error("Must specify CPU (-c) or GPU (-g), but not both.");
        return 1;
    }

    // Close file, if open.
    if (fh.is_open()) {
        fh.flush();
        fh.close();
    }

    // All done.
    return 0;
}




int main(int argc, char** argv) {
    // Listen for shutdown.
    std::signal(SIGINT, shutdown_signal_handler);
    std::signal(SIGTERM, shutdown_signal_handler);



    // Initialize the logger.
    init_logger("log/verifier.log");



    // Process options.
    bool verbose = false;
    LaunchContext ctx;
    CLI::App options("Verify Collatz sequence convergence of surviving values from the provided tree.");
    options.add_flag("-c,--cpu", ctx.use_cpu, "Use the CPU for verification.");
    options.add_flag("-d,--detailed-metrics", ctx.detailed_metrics, "Enable detailed metrics.  Very slow.  CPU only.");
    options.add_option("-e,--end", ctx.end_s, "End value to test to.  Guaranteed minimum, might exceed slightly.  When 0, stops at bit limit (or infinite if mpz_class).");
    options.add_flag("-g,--gpu", ctx.use_gpu, "Use the GPU for verification.");
    options.add_option("-G,--gpu-memory-limit", ctx.gpu_memory_limit, "Restrict GPU memory to X bytes.");
    options.add_option("-i,--ilp-path", ctx.ilp_path, "Emit metrics in Influx Line Protocol to the path specified (append mode).");
    options.add_flag("-n,--no-iv-table", ctx.no_iv_table, "Disables the Initial Value table.");
    options.add_option("-s,--start", ctx.start_s, "Start value to begin with.  If lower than tree, will be bumped up.");
    options.add_option("-S,--scales-per-run", ctx.gpu_scales_per_run, "Set the scales-per-run value to X for GPU verifiers.  Reduces CPU and PCIe overhead.  See verifier_gpu.hpp for details.");
    options.add_option("-t,--tree", ctx.tree_path, "Path to an existing tree export to use.  Can be built with bin/tree_builder.");
    options.add_option("-u,--update-ms", ctx.update_ms, "Update the UI every X milliseconds.  Controls ILP (-i) frequency too.");
    options.add_flag(
        "-v,--verbose"
        , [&](size_t x){if(x>0) {verbose=true; logger->set_level(spdlog::level::debug);}}
        , "Enable verbosity."
    );
    CLI11_PARSE(options, argc, argv);
    logger->debug("Selected options were:");
    logger->debug("  Use CPU             (-c): {}", ctx.use_cpu);
    logger->debug("  Detailed Metrics    (-d): {}", ctx.detailed_metrics);
    logger->debug("  End Value           (-e): {}", ctx.end_s);
    logger->debug("  Use GPU             (-g): {}", ctx.use_gpu);
    logger->debug("  GPU Memory Limit    (-G): {}", verbose);
    logger->debug("  ILP Path            (-i): {}", ctx.ilp_path);
    logger->debug("  No IV Table         (-n): {}", ctx.no_iv_table);
    logger->debug("  Start Value         (-s): {}", ctx.start_s);
    logger->debug("  GPU Scales Per Run  (-S): {}", ctx.gpu_scales_per_run);
    logger->debug("  Tree Path           (-t): {}", ctx.tree_path);
    logger->debug("  Update ms           (-u): {}", ctx.update_ms);
    logger->debug("  Verbose             (-v): {}", verbose);



    // Sanity checks.

    // File has to exist.  This program doesn't build trees.
    if (ctx.tree_path.empty()) {
        logger->error("You must specify a tree file (-t) to load.  These can be built with bin/tree_builder.");
        return 1;
    }
    if (std::filesystem::exists(ctx.tree_path) == false) {
        logger->error("Cannot find the tree file specified: {}", ctx.tree_path);
        return 1;
    }
    std::ifstream test_fh(ctx.tree_path, std::ios::binary);
    if (!test_fh) {
        logger->error("Unable to open tree file specified: {}", ctx.tree_path);
        return 1;
    }
    test_fh.close();

    // Peek into the file and ensure it's valid.  Also tells us the type to call.
    uint32_t version;
    uint8_t tree_type;
    uint16_t tree_type_bit_width_id;
    level_t level_count;
    std::string err;
    if (! BinaryTree<uint64_t>::peek_at_core_attributes(ctx.tree_path, version, tree_type, tree_type_bit_width_id, level_count)) {
        logger->error("Unable to peek at the attributes of the file.  Error follows:");
        logger->error(err);
        return 1;
    }

    // Only allow implicit trees.
    if (tree_type != TreeTypeEnum::IMPLICIT) {
        logger->error("Only Implicit tree types are allowed.");
        return 1;
    }

    // Start value is usually desired.  Will default to last value of tree, but issue a warning.
    if (ctx.start_s.empty()) {
        logger->warn("No start value (-s) specified.  Will default to the last values of the tree.");
        ctx.start_s = "0";
    }

    // End value is usually desired.  Will default to type-T max, or infinite if mpz_class.
    if (ctx.end_s.empty()) {
        logger->warn("No end value (-e) specified.  Will default to type max, or infinite if mpz_class.");
        ctx.end_s = "0";
    }

    // Either CPU or GPU must be used, but not both.
    if (ctx.use_cpu == true && ctx.use_gpu == true) {
        logger->error("Cannot specify both CPU (-c) and GPU (-g) verification at the same time.");
        return 1;
    }
    if (ctx.use_cpu == false && ctx.use_gpu == false) {
        logger->error("Must specify either CPU (-c) or GPU (-g) verification, but not both.");
        return 1;
    }
    if (ctx.use_gpu == true && GPU::can_use_gpu() == false) {
        logger->error("You requested GPU (-g) verification but no valid GPU was detected.");
        return 1;
    }

    // Juggle types and try to discern which value type matches.  Make sure it fits the tree's type.  This is a hard requirement for now.
    if (tree_type_bit_width_id > 0) {
        // Tree is non-GMP type (fixed-width).  Parse start/end strings and compare to numeric limits.
        mpz_class start_mpz = mpz_class(ctx.start_s);
        mpz_class end_mpz = mpz_class(ctx.end_s);
        uint16_t bits_required = 0;
        if (end_mpz == 0) {
            // End value is zero, try to use the start value.  Warn the user about this.
            logger->warn("No end value (-e) means value-type is deduced from start value (-s) or tree level if no start value was sent either.");
            if (start_mpz == 0) {
                // Start value is zero.  Iteration will begin at the last level of the tree.  So just use that.
                bits_required = level_count;
            } else {
                // Start value is non-zero, which means the type only needs to support it and nothing more.
                bits_required = mpz_sizeinbase(start_mpz.get_mpz_t(), 2);
            }
        } else {
            // End value is non-zero, which means the type only needs to support it and nothing more.
            bits_required = mpz_sizeinbase(end_mpz.get_mpz_t(), 2);
        }

        // Now confirm the bits_required matches the type ID (ID == bit size for fixed-width types).
        if (bits_required > tree_type_bit_width_id) {
            logger->error(
                "The bits required by your start (-s) and/or end (-e) values is {}, which exceeds the data type of the tree (-t) specified: {}.  This is not allowed.  Rebuild the tree with a wider type."
                , bits_required
                , tree_type_bit_width_id
            );
            return 1;
        }
    }



    // Launch
    int result;
    if (tree_type_bit_width_id == 64) {
        result = do_it<uint64_t>(ctx);
    } else if (tree_type_bit_width_id == 128) {
        result = do_it<uint128_t>(ctx);
    } else if (tree_type_bit_width_id == 0) {
        result = do_it<mpz_class>(ctx);
    } else {
        logger->error("Cannot determine which data type to use (64 bit, 128 bit, or GMP) for the tree's bit width ID: {}", tree_type_bit_width_id);
        return 1;
    }

    logger->info("All done.  Exit code: {}.", result);
    return result;

}
