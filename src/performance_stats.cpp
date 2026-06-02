#include <climits>
#include <cstdint>
#include <gmpxx.h>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <omp.h>
#include <ratio>
#include <string>
#include <format>
#include "collatz/binary_tree.hpp"
#include "collatz/binary_tree_materialized.hpp"
#include "collatz/collatz.hpp"
#include "collatz/concepts.hpp"
#include "collatz/for_each.hpp"
#include <fstream>
#include <unistd.h>
#include <CLI/CLI.hpp>


using namespace std::chrono;


// OMP Bits
const int OMP_INITIAL_MAX_THREADS = omp_get_max_threads();



//
// Get RSS Bytes
//
size_t getCurrentRSSBytes() {
    long rss_pages = 0;
    std::ifstream statm("/proc/self/statm");
    if (statm.good()) {
        long dummy;
        statm >> dummy >> rss_pages;
        statm.close();
    }
    long page_size_bytes = sysconf(_SC_PAGE_SIZE); // bytes per page
    return rss_pages * page_size_bytes; // RSS in bytes
}



//
// Table Printing
//
std::vector<size_t> calculateColumnWidths(const std::vector<std::vector<std::string>>& table) {
    if (table.empty()) return {};

    size_t columns = table[0].size();
    std::vector<size_t> colWidths(columns, 0);

    for (const auto& row : table) {
        for (size_t col = 0; col < columns; ++col) {
            if (col < row.size()) {
                colWidths[col] = std::max(colWidths[col], row[col].length());
            }
        }
    }
    return colWidths;
}
//
// Print
void printTable(const std::vector<std::vector<std::string>>& table) {
    std::vector<size_t> colWidths = calculateColumnWidths(table);

    for (size_t i = 0; i < table.size(); ++i) {
        const auto& row = table[i];
        for (size_t col = 0; col < row.size(); ++col) {
            if (col == 0) {
                std::cout << std::left << std::setw(static_cast<int>(colWidths[col])) << row[col];
            } else {
                std::cout << std::right << std::setw(static_cast<int>(colWidths[col])) << row[col];
            }
            std::cout << "  ";
        }
        std::cout << '\n';

        // Print separator after header
        if (i == 0) {
            for (size_t col = 0; col < colWidths.size(); ++col) {
                std::cout << std::string(colWidths[col], '-') << "  ";
            }
            std::cout << '\n';
        }
    }
}


//
// Add Generic line to table output.
//
std::vector<std::string> add(const char *name, int64_t u_version, int64_t u128_version, int64_t z_version, const char* unit="bytes") {
    std::vector<std::string> vector;
    int delta_mpz_abs = z_version - u_version;
    int delta_u128_abs = u128_version - u_version;
    vector.push_back(name);
    vector.push_back(std::to_string(u_version));
    vector.push_back(std::to_string(u128_version));
    vector.push_back(std::to_string(z_version));
    vector.push_back(unit);
    vector.push_back(std::to_string(delta_u128_abs));
    vector.push_back(std::to_string(100.0f * delta_u128_abs / u_version) + '%');
    vector.push_back(std::to_string(delta_mpz_abs));
    vector.push_back(std::to_string(100.0f * delta_mpz_abs / u_version) + '%');
    return vector;
}
//
// Floating Point Version
//
std::vector<std::string> add_f(const char* name, double u_version, double u128_version, double z_version, const char* unit = "bytes") {
    std::vector<std::string> vector;

    double delta_mpz_abs = z_version - u_version;
    double delta_u128_abs = u128_version - u_version;

    vector.push_back(name);
    vector.push_back(std::to_string(u_version));
    vector.push_back(std::to_string(u128_version));
    vector.push_back(std::to_string(z_version));
    vector.push_back(unit);

    vector.push_back(std::to_string(delta_u128_abs));
    vector.push_back(std::to_string(100.0 * delta_u128_abs / u_version) + "%");

    vector.push_back(std::to_string(delta_mpz_abs));
    vector.push_back(std::to_string(100.0 * delta_mpz_abs / u_version) + "%");

    return vector;
}



//
// Tree Build Time
//
template<AnySupportedIntegral T, typename TreeType>
duration<double, std::milli> tree_build_time(level_t levels, const BinaryTreeOptions& opts) {
    auto start = high_resolution_clock::now();
    BinaryTree tree = BinaryTree<T, TreeType>(levels, opts);
    auto end = high_resolution_clock::now();
    duration<double, std::milli> duration = end - start;
    return duration;
}


//
// Value Map Generation Performance
//
struct ValueMapGenStats {
    duration<double, std::milli> duration_milli;
    int64_t duration_ms = 0;
    int64_t uncovered_position_count = 0;
    int64_t uncovered_value_count = 0;
    int64_t conversion_rate = 0;
    double position_bytes_per_node = 0;
    double value_bytes_per_node = 0;
};
template<AnySupportedIntegral T>
ValueMapGenStats value_map_generation_stats(ImplicitBinaryTree<T>& tree, ForEachPolicy policy) {
    tree.clear_uncovered_values();
    auto start = high_resolution_clock::now();
    tree.generate_value_map(policy);
    auto end = high_resolution_clock::now();
    duration<double, std::milli> duration = end - start;

    // Build the stats.
    ValueMapGenStats stats;
    stats.duration_milli = duration;
    stats.duration_ms = duration.count();
    if constexpr(GMPIntegral<T>) {
        stats.uncovered_position_count = tree.get_uncovered_positions().cardinality().get_ui();
        stats.uncovered_value_count = tree.get_uncovered_values().cardinality().get_ui();
    } else {
        stats.uncovered_position_count = tree.get_uncovered_positions().cardinality();
        stats.uncovered_value_count = tree.get_uncovered_values().cardinality();
    }
    stats.conversion_rate = stats.duration_ms == 0 ? 0 : stats.uncovered_position_count / stats.duration_ms;
    stats.position_bytes_per_node = 1.0 * tree.get_uncovered_positions().deep_size() / stats.uncovered_position_count;
    stats.value_bytes_per_node = 1.0 * tree.get_uncovered_values().deep_size() / stats.uncovered_value_count;

    return stats;
}



//
// Value Map Iteration Performance
//
template<AnySupportedIntegral T>
int64_t value_map_iteration_per_ms(ImplicitBinaryTree<T>& tree, ForEachPolicy policy, uint64_t target) {
    uint64_t final_target = target;
    if (policy == ForEachPolicy::SERIAL) {
        // Cut the test in half for the slower method.
        final_target = target / 2;
    }
    if constexpr(GMPIntegral<T>) {
        // MPZ is ~10x slower.
        final_target = target / 10;
    }
    uint64_t dummy = 0;
    std::atomic<uint64_t> a_dummy = 0;

    auto start = high_resolution_clock::now();
    auto end = high_resolution_clock::now();
    if (policy == ForEachPolicy::SERIAL) {
        // Serial test.  Can just loop until count hits target.
        uint64_t count = 0;
        start = high_resolution_clock::now();
        tree.for_each_uncovered_value(policy, [&](const T& value) {
            // Comput something cheap-ish that can't be optimized-away at compile or runtime.
            if constexpr(FixedWidthIntegral<T>) {
                if (value & 0) {
                    dummy = dummy + 1;
                }
            } else if constexpr(GMPIntegral<T>) {
                if (mpz_even_p(value.get_mpz_t())) {
                    dummy = dummy + 1;
                }
            }
            count++;
            if (count >= final_target) {
                return ForEachSignal::BREAK;
            }
            return ForEachSignal::CONTINUE;
        });
        end = high_resolution_clock::now();
        assert(dummy == 0);
    } else {
        // Parallel test.  Drag the target back by omp count to attempt to hit the value above since it's spread among threads.
        final_target /= omp_get_max_threads();
        std::vector<uint64_t> count_vec;
        start = high_resolution_clock::now();
        tree.for_each_uncovered_value_with_tls(policy, count_vec, [&](const T& value, uint64_t& tls_count) {
            // Comput something cheap-ish that can't be optimized-away at compile or runtime.
            if constexpr(FixedWidthIntegral<T>) {
                if (value & 0) {
                    dummy = dummy + 1;
                }
            } else if constexpr(GMPIntegral<T>) {
                if (mpz_even_p(value.get_mpz_t())) {
                    dummy = dummy + 1;
                }
            }
            tls_count++;
            if (tls_count >= final_target) {
                return ForEachSignal::BREAK;
            }
            return ForEachSignal::CONTINUE;
        });
        end = high_resolution_clock::now();
        // Update the target per-thread.
        assert(a_dummy.load() == 0);
        final_target = 0;
        for (const uint64_t& tls_count : count_vec) {
            final_target += tls_count;
        }
    }
    duration<double, std::milli> duration = end - start;
    return final_target / duration.count();
}



// Helper struct for Binary Tree stats to avoid allocating a crap ton of memory.
template<AnySupportedIntegral T>
struct BTStats {
    int64_t rss = 0;
    int64_t deep_size = 0;
    T node_count = 0;
    int64_t bytes_per_node = 0;
    int64_t rss_bytes_per_node = 0;
};
// Now the method to fill it.
template<AnySupportedIntegral T, typename TreeType>
BTStats<T> get_bt_stats(level_t levels, BinaryTreeOptions opts) {
    // Start watching RSS.  This isn't very reasonable, but whatever.
    int64_t rss_t1 = getCurrentRSSBytes();

    // Build the tree.
    BinaryTree<T, TreeType> tree(levels, opts);

    // Capture the stats.
    BTStats<T> stats;
    stats.rss = getCurrentRSSBytes() - rss_t1;
    stats.deep_size = tree.deep_size();
    stats.node_count = tree.node_count();
    if constexpr(GMPIntegral<T>) {
        stats.bytes_per_node = stats.deep_size / stats.node_count.get_ui();
        stats.rss_bytes_per_node = stats.rss / stats.node_count.get_ui();
    } else {
        stats.bytes_per_node = stats.deep_size / stats.node_count;
        stats.rss_bytes_per_node = stats.rss / stats.node_count;
    }

    // Return them.
    return stats;
}





int main(int argc, char **argv) {
    CLI::App options("Generates performance stats for internal diagnostics of our code.");
    size_t levels;
    size_t max_threads;
    size_t iteration_target;
    bool compare_threads = false;
    bool skip_materialized = false;
    options.add_flag("-c,--compare-threads", compare_threads, "Compare tree-building times thread-by-thread (show deltas).");
    options.add_option("-i,--iterations", iteration_target, "How many Tree (NodeBitmap) iterations to test.  MPZ tests 0.1x this.  Serial tests 0.5x this.")->default_val(1'000'000'000);
    options.add_option("-l,--levels", levels, "How many levels to build trees.")->default_val(24);
    options.add_flag("-s,--skip-materialized", skip_materialized, "Skip materialized trees to reduce memory and focus on Implicit trees.");
    options.add_option("-t,--threads", max_threads, "How many threads to test with.")->default_val(4);
    CLI11_PARSE(options, argc, argv);

    // Size Data
    std::cout << "Building objects for size data with " << levels << " levels..." << std::flush;
    Collatz collatz_uint64_t = Collatz<uint64_t>(27);
    Collatz collatz_uint128_t = Collatz<uint128_t>(27);
    Collatz collatz_mpz_c = Collatz<mpz_class>(27);
    Node node_uint64_t = Node<uint64_t>(27);
    Node node_uint128_t = Node<uint128_t>(27);
    Node node_mpz_c = Node<mpz_class>(27);

    // Binrary Tree Option Sets
    BinaryTreeOptions BTWithoutPrune = {.prune_hwm_nodes = false };
    BinaryTreeOptions BTWithPrune = {.prune_hwm_nodes = true, .prune_parent_levels = true};
    BinaryTreeOptions DEFAULT_OPTS;

    // BT without pruning
    BTStats<uint64_t> tree_uint64_t_without_pruning_stats;
    BTStats<uint128_t> tree_uint128_t_without_pruning_stats;
    BTStats<mpz_class> tree_mpz_c_without_pruning_stats;
    if (! skip_materialized) {
        tree_uint64_t_without_pruning_stats = get_bt_stats<uint64_t, BinaryTreeMaterializedImpl<uint64_t>>(levels, BTWithoutPrune);
        tree_uint128_t_without_pruning_stats = get_bt_stats<uint128_t, BinaryTreeMaterializedImpl<uint128_t>>(levels, BTWithoutPrune);
        tree_mpz_c_without_pruning_stats = get_bt_stats<mpz_class, BinaryTreeMaterializedImpl<mpz_class>>(levels, BTWithoutPrune);
    }

    // BT with pruning
    BTStats<uint64_t> tree_uint64_t_with_pruning_stats;
    BTStats<uint128_t> tree_uint128_t_with_pruning_stats;
    BTStats<mpz_class> tree_mpz_c_with_pruning_stats;
    if (! skip_materialized) {
        tree_uint64_t_with_pruning_stats = get_bt_stats<uint64_t, BinaryTreeMaterializedImpl<uint64_t>>(levels, BTWithPrune);
        tree_uint128_t_with_pruning_stats = get_bt_stats<uint128_t, BinaryTreeMaterializedImpl<uint128_t>>(levels, BTWithPrune);
        tree_mpz_c_with_pruning_stats = get_bt_stats<mpz_class, BinaryTreeMaterializedImpl<mpz_class>>(levels, BTWithPrune);
    }

    // BT Implicit
    BTStats<uint64_t> tree_uint64_t_implicit_stats = get_bt_stats<uint64_t, BinaryTreeImplicitImpl<uint64_t>>(levels, DEFAULT_OPTS);
    BTStats<uint128_t> tree_uint128_t_implicit_stats = get_bt_stats<uint128_t, BinaryTreeImplicitImpl<uint128_t>>(levels, DEFAULT_OPTS);
    BTStats<mpz_class> tree_mpz_c_implicit_stats = get_bt_stats<mpz_class, BinaryTreeImplicitImpl<mpz_class>>(levels, DEFAULT_OPTS);

    std::cout << " done." << std::endl;

    // Rate Data
    ImplicitBinaryTree<uint64_t> implicit_tree_uint64_t(levels);
    ImplicitBinaryTree<uint128_t> implicit_tree_uint128_t(levels);
    ImplicitBinaryTree<mpz_class> implicit_tree_mpz_c(levels);
    std::vector<duration<double, std::milli>> duration_uint64_t_without_pruning(max_threads);
    std::vector<duration<double, std::milli>> duration_uint128_t_without_pruning(max_threads);
    std::vector<duration<double, std::milli>> duration_mpz_class_without_pruning(max_threads);
    std::vector<duration<double, std::milli>> duration_uint64_t_with_pruning(max_threads);
    std::vector<duration<double, std::milli>> duration_uint128_t_with_pruning(max_threads);
    std::vector<duration<double, std::milli>> duration_mpz_class_with_pruning(max_threads);
    std::vector<duration<double, std::milli>> duration_uint64_t_implicit(max_threads);
    std::vector<duration<double, std::milli>> duration_uint128_t_implicit(max_threads);
    std::vector<duration<double, std::milli>> duration_mpz_class_implicit(max_threads);
    std::cout << "Benchmarking tree building..." << std::flush;
    for (size_t i = 0; i < max_threads; i++) {
        omp_set_num_threads(i + 1);
        if (! skip_materialized) {
            duration_uint64_t_without_pruning[i] = tree_build_time<uint64_t, BinaryTreeMaterializedImpl<uint64_t>>(levels, BTWithoutPrune);
            duration_uint128_t_without_pruning[i] = tree_build_time<uint128_t, BinaryTreeMaterializedImpl<uint128_t>>(levels, BTWithoutPrune);
            duration_mpz_class_without_pruning[i] = tree_build_time<mpz_class, BinaryTreeMaterializedImpl<mpz_class>>(levels, BTWithoutPrune);
            duration_uint64_t_with_pruning[i] = tree_build_time<uint64_t, BinaryTreeMaterializedImpl<uint64_t>>(levels, BTWithPrune);
            duration_uint128_t_with_pruning[i] = tree_build_time<uint128_t, BinaryTreeMaterializedImpl<uint128_t>>(levels, BTWithPrune);
            duration_mpz_class_with_pruning[i] = tree_build_time<mpz_class, BinaryTreeMaterializedImpl<mpz_class>>(levels, BTWithPrune);
        }
        duration_uint64_t_implicit[i] = tree_build_time<uint64_t, BinaryTreeImplicitImpl<uint64_t>>(levels, DEFAULT_OPTS);
        duration_uint128_t_implicit[i] = tree_build_time<uint128_t, BinaryTreeImplicitImpl<uint128_t>>(levels, DEFAULT_OPTS);
        duration_mpz_class_implicit[i] = tree_build_time<mpz_class, BinaryTreeImplicitImpl<mpz_class>>(levels, DEFAULT_OPTS);
    }
    omp_set_num_threads(OMP_INITIAL_MAX_THREADS);
    std::cout << " done." << std::endl;

    // Value Conversion
    std::cout << "Benchmarking value map generation..." << std::flush;
    ValueMapGenStats value_map_stats_uint64_t_serial = value_map_generation_stats<uint64_t>(implicit_tree_uint64_t, ForEachPolicy::SERIAL);
    ValueMapGenStats value_map_stats_uint128_t_serial = value_map_generation_stats<uint128_t>(implicit_tree_uint128_t, ForEachPolicy::SERIAL);
    ValueMapGenStats value_map_stats_mpz_c_serial = value_map_generation_stats<mpz_class>(implicit_tree_mpz_c, ForEachPolicy::SERIAL);
    ValueMapGenStats value_map_stats_uint64_t_parallel = value_map_generation_stats<uint64_t>(implicit_tree_uint64_t, ForEachPolicy::PARALLEL);
    ValueMapGenStats value_map_stats_uint128_t_parallel = value_map_generation_stats<uint128_t>(implicit_tree_uint128_t, ForEachPolicy::PARALLEL);
    ValueMapGenStats value_map_stats_mpz_c_parallel = value_map_generation_stats<mpz_class>(implicit_tree_mpz_c, ForEachPolicy::PARALLEL);
    std::cout << " done." << std::endl;

    std::cout << "Benchmarking value map iteration..." << std::flush;
    if (implicit_tree_uint64_t.get_uncovered_values().empty()) {
        implicit_tree_uint64_t.generate_value_map();
    }
    if (implicit_tree_uint128_t.get_uncovered_values().empty()) {
        implicit_tree_uint128_t.generate_value_map();
    }
    if (implicit_tree_mpz_c.get_uncovered_values().empty()) {
        implicit_tree_mpz_c.generate_value_map();
    }
    std::vector<int64_t> uncovered_value_iteration_uint64_t_per_ms_serial(max_threads);
    std::vector<int64_t> uncovered_value_iteration_uint128_t_per_ms_serial(max_threads);
    std::vector<int64_t> uncovered_value_iteration_mpz_c_per_ms_serial(max_threads);
    std::vector<int64_t> uncovered_value_iteration_uint64_t_per_ms_parallel(max_threads);
    std::vector<int64_t> uncovered_value_iteration_uint128_t_per_ms_parallel(max_threads);
    std::vector<int64_t> uncovered_value_iteration_mpz_c_per_ms_parallel(max_threads);
    for (size_t i = 0; i < max_threads; i++) {
        omp_set_num_threads(i + 1);
        uncovered_value_iteration_uint64_t_per_ms_serial[i] = value_map_iteration_per_ms<uint64_t>(implicit_tree_uint64_t, ForEachPolicy::SERIAL, iteration_target);
        uncovered_value_iteration_uint128_t_per_ms_serial[i] = value_map_iteration_per_ms<uint128_t>(implicit_tree_uint128_t, ForEachPolicy::SERIAL, iteration_target);
        uncovered_value_iteration_mpz_c_per_ms_serial[i] = value_map_iteration_per_ms<mpz_class>(implicit_tree_mpz_c, ForEachPolicy::SERIAL, iteration_target);
        uncovered_value_iteration_uint64_t_per_ms_parallel[i] = value_map_iteration_per_ms<uint64_t>(implicit_tree_uint64_t, ForEachPolicy::PARALLEL, iteration_target);
        uncovered_value_iteration_uint128_t_per_ms_parallel[i] = value_map_iteration_per_ms<uint128_t>(implicit_tree_uint128_t, ForEachPolicy::PARALLEL, iteration_target);
        uncovered_value_iteration_mpz_c_per_ms_parallel[i] = value_map_iteration_per_ms<mpz_class>(implicit_tree_mpz_c, ForEachPolicy::PARALLEL, iteration_target);
    }
    std::cout << " done." << std::endl;
    omp_set_num_threads(OMP_INITIAL_MAX_THREADS);


    // Print Table
    std::vector<std::vector<std::string>> table;
    table.push_back({"Item", "uint64_t", "uint128_t", "mpz_class", "Unit", "u128 Delta", "u128 Delta (+%)", "MPZ Delta", "MPZ Delta (+%)"});
    table.push_back(add("Base Data Type", sizeof(uint64_t), sizeof(uint128_t), sizeof(mpz_class)));

    // Collatz
    table.push_back({""});
    table.push_back(add("Collatz (shallow)", sizeof(Collatz<uint64_t>), sizeof(Collatz<uint128_t>), sizeof(Collatz<mpz_class>)));
    table.push_back(add("Collatz (deep)", collatz_uint64_t.deep_size(), collatz_uint128_t.deep_size(), collatz_mpz_c.deep_size()));

    // Node
    table.push_back({""});
    table.push_back(add("Node (shallow)", sizeof(Node<uint64_t>), sizeof(Node<uint128_t>), sizeof(Node<mpz_class>)));
    table.push_back(add("Node(27) (deep)", node_uint64_t.deep_size(), node_uint128_t.deep_size(), node_mpz_c.deep_size()));

    // BinaryTree
    table.push_back({""});
    table.push_back(add("BinaryTree (shallow)", sizeof(BinaryTree<uint64_t>), sizeof(BinaryTree<uint128_t>), sizeof(BinaryTree<mpz_class>)));
    table.push_back({std::format("BinaryTree (deep, {} levels)", levels).c_str(), "--", "--", "--", "--", "--", "--", "--", "--"});
    table.push_back(add("  Materialized, Without Pruning", tree_uint64_t_without_pruning_stats.deep_size, tree_uint128_t_without_pruning_stats.deep_size, tree_mpz_c_without_pruning_stats.deep_size));
    table.push_back(add("    As Megabytes", tree_uint64_t_without_pruning_stats.deep_size/1024/1024, tree_uint128_t_without_pruning_stats.deep_size/1024/1024, tree_mpz_c_without_pruning_stats.deep_size/1024/1024, "Mbytes"));
    table.push_back(add("    As Gigabytes", tree_uint64_t_without_pruning_stats.deep_size/1024/1024/1024, tree_uint128_t_without_pruning_stats.deep_size/1024/1024/1024, tree_mpz_c_without_pruning_stats.deep_size/1024/1024/1024, "Gbytes"));
    table.push_back(add("    According to RSS", tree_uint64_t_without_pruning_stats.rss, tree_uint128_t_without_pruning_stats.rss, tree_mpz_c_without_pruning_stats.rss));
    table.push_back(add("    Nodes", tree_uint64_t_without_pruning_stats.node_count, tree_uint128_t_without_pruning_stats.node_count, tree_mpz_c_without_pruning_stats.node_count.get_ui(), "nodes"));
    table.push_back(add("      Bytes per Node (Internal Tracking)", tree_uint64_t_without_pruning_stats.bytes_per_node, tree_uint128_t_without_pruning_stats.bytes_per_node, tree_mpz_c_without_pruning_stats.bytes_per_node));
    table.push_back(add("      Bytes per Node (RSS Usage)", tree_uint64_t_without_pruning_stats.rss_bytes_per_node, tree_uint128_t_without_pruning_stats.rss_bytes_per_node, tree_mpz_c_without_pruning_stats.rss_bytes_per_node));
    table.push_back(add("      Difference: RSS - Internal", tree_uint64_t_without_pruning_stats.rss_bytes_per_node - tree_uint64_t_without_pruning_stats.bytes_per_node, tree_uint128_t_without_pruning_stats.rss_bytes_per_node - tree_uint128_t_without_pruning_stats.bytes_per_node, tree_mpz_c_without_pruning_stats.rss_bytes_per_node - tree_mpz_c_without_pruning_stats.bytes_per_node));
    table.push_back(add("  Materialized, With Pruning", tree_uint64_t_with_pruning_stats.deep_size, tree_uint128_t_with_pruning_stats.deep_size, tree_mpz_c_with_pruning_stats.deep_size));
    table.push_back(add("    As Megabytes", tree_uint64_t_with_pruning_stats.deep_size/1024/1024, tree_uint128_t_with_pruning_stats.deep_size/1024/1024, tree_mpz_c_with_pruning_stats.deep_size/1024/1024, "Mbytes"));
    table.push_back(add("    As Gigabytes", tree_uint64_t_with_pruning_stats.deep_size/1024/1024/1024, tree_uint128_t_with_pruning_stats.deep_size/1024/1024/1024, tree_mpz_c_with_pruning_stats.deep_size/1024/1024/1024, "Gbytes"));
    table.push_back(add("    According to RSS", tree_uint64_t_with_pruning_stats.rss, tree_uint128_t_with_pruning_stats.rss, tree_mpz_c_with_pruning_stats.rss));
    table.push_back(add("    Nodes", tree_uint64_t_with_pruning_stats.node_count, tree_uint128_t_with_pruning_stats.node_count, tree_mpz_c_with_pruning_stats.node_count.get_ui(), "nodes"));
    table.push_back(add("      Bytes per Node (Internal Tracking)", tree_uint64_t_with_pruning_stats.bytes_per_node, tree_uint128_t_with_pruning_stats.bytes_per_node, tree_mpz_c_with_pruning_stats.bytes_per_node));
    table.push_back(add("      Bytes per Node (RSS Usage)", tree_uint64_t_with_pruning_stats.rss_bytes_per_node, tree_uint128_t_with_pruning_stats.rss_bytes_per_node, tree_mpz_c_with_pruning_stats.rss_bytes_per_node));
    table.push_back(add("      Difference: RSS - Internal", tree_uint64_t_with_pruning_stats.rss_bytes_per_node - tree_uint64_t_with_pruning_stats.bytes_per_node, tree_uint128_t_with_pruning_stats.rss_bytes_per_node - tree_uint128_t_with_pruning_stats.bytes_per_node, tree_mpz_c_with_pruning_stats.rss_bytes_per_node - tree_mpz_c_with_pruning_stats.bytes_per_node));
    table.push_back(add("  Implicit", tree_uint64_t_implicit_stats.deep_size, tree_uint128_t_implicit_stats.deep_size, tree_mpz_c_implicit_stats.deep_size));
    table.push_back(add("    As Megabytes", tree_uint64_t_implicit_stats.deep_size/1024/1024, tree_uint128_t_implicit_stats.deep_size/1024/1024, tree_mpz_c_implicit_stats.deep_size/1024/1024, "Mbytes"));
    table.push_back(add("    As Gigabytes", tree_uint64_t_implicit_stats.deep_size/1024/1024/1024, tree_uint128_t_implicit_stats.deep_size/1024/1024/1024, tree_mpz_c_implicit_stats.deep_size/1024/1024/1024, "Gbytes"));
    table.push_back(add("    According to RSS", tree_uint64_t_implicit_stats.rss, tree_uint128_t_implicit_stats.rss, tree_mpz_c_implicit_stats.rss));
    table.push_back(add("    Nodes", tree_uint64_t_implicit_stats.node_count, tree_uint128_t_implicit_stats.node_count, tree_mpz_c_implicit_stats.node_count.get_ui(), "nodes"));
    table.push_back(add("      Bytes per Node (Internal Tracking)", tree_uint64_t_implicit_stats.bytes_per_node, tree_uint128_t_implicit_stats.bytes_per_node, tree_mpz_c_implicit_stats.bytes_per_node));
    table.push_back(add("      Bytes per Node (RSS Usage)", tree_uint64_t_implicit_stats.rss_bytes_per_node, tree_uint128_t_implicit_stats.rss_bytes_per_node, tree_mpz_c_implicit_stats.rss_bytes_per_node));
    table.push_back(add("      Difference: RSS - Internal", tree_uint64_t_implicit_stats.rss_bytes_per_node - tree_uint64_t_implicit_stats.bytes_per_node, tree_uint128_t_implicit_stats.rss_bytes_per_node - tree_uint128_t_implicit_stats.bytes_per_node, tree_mpz_c_implicit_stats.rss_bytes_per_node - tree_mpz_c_implicit_stats.bytes_per_node));

    // Compare tree sizes
    // This is tricky because we need floats.
    double u64ratio_unpruned_vs_pruned = 100.0 * tree_uint64_t_without_pruning_stats.deep_size / tree_uint64_t_with_pruning_stats.deep_size;
    double u128_ratio_unpruned_vs_pruned = 100.0 * tree_uint128_t_without_pruning_stats.deep_size / tree_uint128_t_with_pruning_stats.deep_size;
    double mpz_ratio_unpruned_vs_pruned = 100.0 * tree_mpz_c_without_pruning_stats.deep_size / tree_mpz_c_with_pruning_stats.deep_size;
    double u64ratio_implicit_vs_pruned = 100.0 * tree_uint64_t_implicit_stats.deep_size / tree_uint64_t_with_pruning_stats.deep_size;
    double u128_ratio_implicit_vs_pruned = 100.0 * tree_uint128_t_implicit_stats.deep_size / tree_uint128_t_with_pruning_stats.deep_size;
    double mpz_ratio_implicit_vs_pruned = 100.0 * tree_mpz_c_implicit_stats.deep_size / tree_mpz_c_with_pruning_stats.deep_size;
    table.push_back({"  Size Comparison", "--", "--", "--", "--", "--", "--", "--", "--"});
    table.push_back({"    Unpruned vs Pruned", std::to_string(u64ratio_unpruned_vs_pruned), std::to_string(u128_ratio_unpruned_vs_pruned), std::to_string(mpz_ratio_unpruned_vs_pruned), "%", "--", "--", "--", "--"});
    table.push_back({"    Implicit vs Pruned", std::to_string(u64ratio_implicit_vs_pruned), std::to_string(u128_ratio_implicit_vs_pruned), std::to_string(mpz_ratio_implicit_vs_pruned), "%", "--", "--", "--", "--"});

    // Now rate data.
    // -- Without Pruning
    table.push_back({"  Build Time Comparison", "--", "--", "--", "--", "--", "--", "--", "--"});
    for (size_t i = 0; i < max_threads; i++) {
        std::string name = "    Without Pruning, " + std::to_string(i + 1) + " threads";
        table.push_back(add(name.c_str(), duration_uint64_t_without_pruning[i].count(), duration_uint128_t_without_pruning[i].count(), duration_mpz_class_without_pruning[i].count(), "ms"));
        if (compare_threads) {
            for (size_t j = 0; j < i; j++) {
                std::string comparison = "      Vs. " + std::to_string(j + 1) + " threads";
                int delta_uint64_t = duration_uint64_t_without_pruning[i].count() - duration_uint64_t_without_pruning[j].count();
                int delta_uint128_t = duration_uint128_t_without_pruning[i].count() - duration_uint128_t_without_pruning[j].count();
                int delta_mpz_class = duration_mpz_class_without_pruning[i].count() - duration_mpz_class_without_pruning[j].count();
                table.push_back({comparison.c_str(), std::to_string(delta_uint64_t), std::to_string(delta_uint128_t), std::to_string(delta_mpz_class), "ms", "--", "--", "--", "--"});
            }
        }
    }
    // -- With Pruning
    for (size_t i = 0; i < max_threads; i++) {
        std::string name = "    With Pruning, " + std::to_string(i + 1) + " threads";
        table.push_back(add(name.c_str(), duration_uint64_t_with_pruning[i].count(), duration_uint128_t_with_pruning[i].count(), duration_mpz_class_with_pruning[i].count(), "ms"));
        if (compare_threads) {
            for (size_t j = 0; j < i; j++) {
                std::string comparison = "      Vs. " + std::to_string(j + 1) + " threads";
                int delta_uint64_t = duration_uint64_t_with_pruning[i].count() - duration_uint64_t_with_pruning[j].count();
                int delta_uint128_t = duration_uint128_t_with_pruning[i].count() - duration_uint128_t_with_pruning[j].count();
                int delta_mpz_class = duration_mpz_class_with_pruning[i].count() - duration_mpz_class_with_pruning[j].count();
                table.push_back({comparison.c_str(), std::to_string(delta_uint64_t), std::to_string(delta_uint128_t), std::to_string(delta_mpz_class), "ms", "--", "--", "--", "--"});
            }
        }
    }
    // -- Implicit
    for (size_t i = 0; i < max_threads; i++) {
        std::string name = "    Implicit, " + std::to_string(i + 1) + " threads";
        table.push_back(add(name.c_str(), duration_uint64_t_implicit[i].count(), duration_uint128_t_implicit[i].count(), duration_mpz_class_implicit[i].count(), "ms"));
        if (compare_threads) {
            for (size_t j = 0; j < i; j++) {
                std::string comparison = "      Vs. " + std::to_string(j + 1) + " threads";
                int delta_uint64_t = duration_uint64_t_implicit[i].count() - duration_uint64_t_implicit[j].count();
                int delta_uint128_t = duration_uint128_t_implicit[i].count() - duration_uint128_t_implicit[j].count();
                int delta_mpz_class = duration_mpz_class_implicit[i].count() - duration_mpz_class_implicit[j].count();
                table.push_back({comparison.c_str(), std::to_string(delta_uint64_t), std::to_string(delta_uint128_t), std::to_string(delta_mpz_class), "ms", "--", "--", "--", "--"});
            }
        }
    }

    // NodeBitmap Performance
    table.push_back({""});
    table.push_back(add("NodeBitmaps (from tree)", NodeBitmap<uint64_t>().deep_size(), NodeBitmap<uint128_t>().deep_size(), NodeBitmap<mpz_class>().deep_size()));
    table.push_back(add("  Uncovered Positions", value_map_stats_uint64_t_serial.uncovered_position_count, value_map_stats_uint128_t_serial.uncovered_position_count, value_map_stats_mpz_c_serial.uncovered_position_count, "nodes"));
    table.push_back(add("  Uncovered Values", value_map_stats_uint64_t_serial.uncovered_value_count, value_map_stats_uint128_t_serial.uncovered_value_count, value_map_stats_mpz_c_serial.uncovered_value_count, "nodes"));
    table.push_back(add_f("  Bytes Per Uncovered Position", value_map_stats_uint64_t_serial.position_bytes_per_node, value_map_stats_uint128_t_serial.position_bytes_per_node, value_map_stats_mpz_c_serial.position_bytes_per_node));
    table.push_back(add_f("  Bytes Per Uncovered Value", value_map_stats_uint64_t_serial.value_bytes_per_node, value_map_stats_uint128_t_serial.value_bytes_per_node, value_map_stats_mpz_c_serial.value_bytes_per_node));
    table.push_back(add("  Value Conversion Rate (Serial)", value_map_stats_uint64_t_serial.conversion_rate, value_map_stats_uint128_t_serial.conversion_rate, value_map_stats_mpz_c_serial.conversion_rate, "nodes/ms"));
    table.push_back(add("  Value Conversion Rate (Parallel)", value_map_stats_uint64_t_parallel.conversion_rate, value_map_stats_uint128_t_parallel.conversion_rate, value_map_stats_mpz_c_parallel.conversion_rate, "nodes/ms"));
    // Iteration Performance (from Tree, but it's a NodeBitmap iterator underneath).
    table.push_back({"  Iteration Performance"});
    for(size_t i = 0; i < max_threads; i++) {
        std::string name = "    Serial, " + std::to_string(i + 1) + " threads";
        table.push_back(add(name.c_str(), uncovered_value_iteration_uint64_t_per_ms_serial[i], uncovered_value_iteration_uint128_t_per_ms_serial[i], uncovered_value_iteration_mpz_c_per_ms_serial[i], "nodes/ms"));
    }
    for(size_t i = 0; i < max_threads; i++) {
        std::string name = "    Parallel, " + std::to_string(i + 1) + " threads";
        table.push_back(add(name.c_str(), uncovered_value_iteration_uint64_t_per_ms_parallel[i], uncovered_value_iteration_uint128_t_per_ms_parallel[i], uncovered_value_iteration_mpz_c_per_ms_parallel[i], "nodes/ms"));
    }



    // Print it.
    printTable(table);

    // All done.
    return 0;
}
