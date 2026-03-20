#include <climits>
#include <cstdint>
#include <gmpxx.h>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ratio>
#include <string>
#include <format>
#include "collatz/binary_tree.hpp"
#include "collatz/collatz.hpp"
#include "collatz/concepts.hpp"
#include <fstream>
#include <unistd.h>
#include "CLI.hpp"
#include "collatz/sieve.hpp"
using namespace std::chrono;

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
// Tree Build Time
//
template<AnySupportedIntegral T>
duration<double, std::milli> tree_build_time(size_t levels, const BinaryTreeOptions& opts) {
    auto start = high_resolution_clock::now();
    NodeBitmap tree = NodeBitmap<T>(levels, opts);
    auto end = high_resolution_clock::now();
    duration<double, std::milli> duration = end - start;
    return duration;
}



//
// Sieve Iteration Time
//
struct sieve_timings {
    duration<double, std::milli> iterator;
    duration<double, std::micro> pool_refill;
    size_t refills;
    size_t refill_fill_loops;
    size_t premature_refills;
};
template<AnySupportedIntegral T>
sieve_timings sieve_iteration_time(
    size_t levels
    , const SieveOptions& opts
    , size_t iterations
    , size_t batch_size
) {
    Sieve<T> sieve(levels, opts);
    T result;
    T dumb_work = 0;
    std::vector<T> bulk(batch_size);
    auto start = high_resolution_clock::now();
    if (batch_size == 1) {
        for (size_t i = 0; i < iterations; i += batch_size) {
            sieve.next(result);
            if(result > 0) { dumb_work += 1; }
        }
    } else {
        for (size_t i = 0; i < iterations; i += batch_size) {
            sieve.next(bulk);
            for (size_t j = 0; j < batch_size; j++) {
                if (bulk.at(j) > 0) { dumb_work += 1; }
            }
        }
    }
    auto end = high_resolution_clock::now();
    duration<double, std::milli> duration = end - start;
    sieve_timings st{};
    st.iterator = duration;
    st.pool_refill = sieve.get_pool_refill_time_us();
    st.refills = sieve.get_pool_refills();
    st.refill_fill_loops = sieve.get_pool_refill_fill_loops();
    st.premature_refills = sieve.get_pool_premature_refills();
    return st;
}





int main(int argc, char **argv) {
    CLI::App options("Generates performance stats for internal diagnostics of our code.");
    size_t levels;
    size_t sieve_tree_levels;
    size_t iterations;
    size_t batch_size;
    size_t max_threads;
    bool compare_threads = false;
    options.add_option("-b,--batch-size", batch_size, "Batch size for sieve.next(bulk) loading.")->default_val(1024);
    options.add_flag("-c,--compare-threads", compare_threads, "Compare tree-building times thread-by-thread (show deltas).");
    options.add_option("-i,--iterations", iterations, "How many iterations of the sieve (i.e.: values to retrieve).")->default_val(50000000);
    options.add_option("-l,--levels", levels, "How many levels to build trees.")->default_val(16);
    options.add_option("-L,--sieve-levels", sieve_tree_levels, "How many levels to build the tree for sieves.")->default_val(16);
    options.add_option("-t,--threads", max_threads, "How many threads to test with.")->default_val(4);
    CLI11_PARSE(options, argc, argv);

    // Size Data
    std::cout << "Building objects for size data with " << levels << " levels..." << std::flush;
    Collatz collatz_with_seq_and_metadata_uint64_t = Collatz<uint64_t>(27, true, true);
    Collatz collatz_with_seq_and_metadata_uint128_t = Collatz<uint128_t>(27, true, true);
    Collatz collatz_with_seq_and_metadata_mpz_c = Collatz<mpz_class>(27, true, true);
    Collatz collatz_with_seq_no_metadata_uint64_t = Collatz<uint64_t>(27, true, false);
    Collatz collatz_with_seq_no_metadata_uint128_t = Collatz<uint128_t>(27, true, false);
    Collatz collatz_with_seq_no_metadata_mpz_c = Collatz<mpz_class>(27, true, false);
    Collatz collatz_with_no_seq_metadata_uint64_t = Collatz<uint64_t>(27, false, true);
    Collatz collatz_with_no_seq_metadata_uint128_t = Collatz<uint128_t>(27, false, true);
    Collatz collatz_with_no_seq_metadata_mpz_c = Collatz<mpz_class>(27, false, true);
    Node node_uint64_t = Node<uint64_t>(27);
    Node node_uint128_t = Node<uint128_t>(27);
    Node node_mpz_c = Node<mpz_class>(27);

    // Binrary Tree Option Sets
    BinaryTreeOptions BTWithoutPrune = {.prune_hwm_nodes = false };
    BinaryTreeOptions BTWithPrune = {.prune_hwm_nodes = true, .prune_parent_levels = true};
    BinaryTreeOptions BTImplicit = {.tree_type = BinaryTreeType::IMPLICIT};

    // BT without pruning
    size_t rss_t1 = getCurrentRSSBytes();
    NodeBitmap tree_uint64_t_without_pruning = NodeBitmap<uint64_t>(levels, BTWithoutPrune);
    size_t rss_uint64_t = getCurrentRSSBytes() - rss_t1;
    rss_t1 = getCurrentRSSBytes();
    NodeBitmap tree_uint128_t_without_pruning = NodeBitmap<uint128_t>(levels, BTWithoutPrune);
    size_t rss_uint128_t = getCurrentRSSBytes() - rss_t1;
    rss_t1 = getCurrentRSSBytes();
    NodeBitmap tree_mpz_c_without_pruning = NodeBitmap<mpz_class>(levels, BTWithoutPrune);
    size_t rss_mpz_c = getCurrentRSSBytes() - rss_t1;

    // BT with pruning
    rss_t1 = getCurrentRSSBytes();
    NodeBitmap tree_uint64_t_with_pruning = NodeBitmap<uint64_t>(levels, BTWithPrune);
    size_t rss_uint64_t_with_pruning = getCurrentRSSBytes() - rss_t1;
    rss_t1 = getCurrentRSSBytes();
    NodeBitmap tree_uint128_t_with_pruning = NodeBitmap<uint128_t>(levels, BTWithPrune);
    size_t rss_uint128_t_with_pruning = getCurrentRSSBytes() - rss_t1;
    rss_t1 = getCurrentRSSBytes();
    NodeBitmap tree_mpz_c_with_pruning = NodeBitmap<mpz_class>(levels, BTWithPrune);
    size_t rss_mpz_c_with_pruning = getCurrentRSSBytes() - rss_t1;

    // BT Implicit
    rss_t1 = getCurrentRSSBytes();
    NodeBitmap tree_uint64_t_implicit = NodeBitmap<uint64_t>(levels, BTImplicit);
    size_t rss_uint64_t_implicit = getCurrentRSSBytes() - rss_t1;
    rss_t1 = getCurrentRSSBytes();
    NodeBitmap tree_uint128_t_implicit = NodeBitmap<uint128_t>(levels, BTImplicit);
    size_t rss_uint128_t_implicit = getCurrentRSSBytes() - rss_t1;
    rss_t1 = getCurrentRSSBytes();
    NodeBitmap tree_mpz_c_implicit = NodeBitmap<mpz_class>(levels, BTImplicit);
    size_t rss_mpz_c_implicit = getCurrentRSSBytes() - rss_t1;

    // Bytes Per Node
    size_t bytes_per_node_uint64_t_without_pruning = tree_uint64_t_without_pruning.deep_size() / tree_uint64_t_without_pruning.node_count();
    size_t bytes_per_node_uint128_t_without_pruning = tree_uint128_t_without_pruning.deep_size() / tree_uint128_t_without_pruning.node_count();
    size_t bytes_per_node_mpz_c_without_pruning = tree_mpz_c_without_pruning.deep_size() / tree_mpz_c_without_pruning.node_count().get_ui();
    size_t bytes_per_node_uint64_t_with_pruning = tree_uint64_t_with_pruning.deep_size() / tree_uint64_t_with_pruning.node_count();
    size_t bytes_per_node_uint128_t_with_pruning = tree_uint128_t_with_pruning.deep_size() / tree_uint128_t_with_pruning.node_count();
    size_t bytes_per_node_mpz_c_with_pruning = tree_mpz_c_with_pruning.deep_size() / tree_mpz_c_with_pruning.node_count().get_ui();
    size_t bytes_per_node_uint64_t_implicit = tree_uint64_t_implicit.deep_size() / tree_uint64_t_implicit.node_count();
    size_t bytes_per_node_uint128_t_implicit = tree_uint128_t_implicit.deep_size() / tree_uint128_t_implicit.node_count();
    size_t bytes_per_node_mpz_c_implicit = tree_mpz_c_implicit.deep_size() / tree_mpz_c_implicit.node_count().get_ui();

    // Sieves
    rss_t1 = getCurrentRSSBytes();
    Sieve sieve_uint64_t = Sieve<uint64_t>(sieve_tree_levels);
    size_t rss_uint64_t_sieve = getCurrentRSSBytes() - rss_t1;
    rss_t1 = getCurrentRSSBytes();
    Sieve sieve_uint128_t = Sieve<uint128_t>(sieve_tree_levels);
    size_t rss_uint128_t_sieve = getCurrentRSSBytes() - rss_t1;
    rss_t1 = getCurrentRSSBytes();
    Sieve sieve_mpz_c = Sieve<mpz_class>(sieve_tree_levels);
    size_t rss_mpz_c_sieve = getCurrentRSSBytes() - rss_t1;

    // RSS
    size_t rss_bytes_per_node_uint64_t_without_pruning = rss_uint64_t / tree_uint64_t_without_pruning.node_count();
    size_t rss_bytes_per_node_uint128_t_without_pruning = rss_uint128_t / tree_uint128_t_without_pruning.node_count();
    size_t rss_bytes_per_node_mpz_c_without_pruning = rss_mpz_c / tree_mpz_c_without_pruning.node_count().get_ui();
    size_t rss_bytes_per_node_uint64_t_with_pruning = rss_uint64_t_with_pruning / tree_uint64_t_with_pruning.node_count();
    size_t rss_bytes_per_node_uint128_t_with_pruning = rss_uint128_t_with_pruning / tree_uint128_t_with_pruning.node_count();
    size_t rss_bytes_per_node_mpz_c_with_pruning = rss_mpz_c_with_pruning / tree_mpz_c_with_pruning.node_count().get_ui();
    size_t rss_bytes_per_node_uint64_t_implicit = rss_uint64_t_implicit / tree_uint64_t_implicit.node_count();
    size_t rss_bytes_per_node_uint128_t_implicit = rss_uint128_t_implicit / tree_uint128_t_implicit.node_count();
    size_t rss_bytes_per_node_mpz_c_implicit = rss_mpz_c_implicit / tree_mpz_c_implicit.node_count().get_ui();
    std::cout << " done." << std::endl;

    // Rate Data
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
        duration_uint64_t_without_pruning[i] = tree_build_time<uint64_t>(levels, BTWithoutPrune);
        duration_uint128_t_without_pruning[i] = tree_build_time<uint128_t>(levels, BTWithoutPrune);
        duration_mpz_class_without_pruning[i] = tree_build_time<mpz_class>(levels, BTWithoutPrune);
        duration_uint64_t_with_pruning[i] = tree_build_time<uint64_t>(levels, BTWithPrune);
        duration_uint128_t_with_pruning[i] = tree_build_time<uint128_t>(levels, BTWithPrune);
        duration_mpz_class_with_pruning[i] = tree_build_time<mpz_class>(levels, BTWithPrune);
        duration_uint64_t_implicit[i] = tree_build_time<uint64_t>(levels, BTImplicit);
        duration_uint128_t_implicit[i] = tree_build_time<uint128_t>(levels, BTImplicit);
        duration_mpz_class_implicit[i] = tree_build_time<mpz_class>(levels, BTImplicit);
    }
    std::cout << " done." << std::endl;

    // Sieve Iterator Data
    std::vector<sieve_timings> duration_sieve_uint64_t(max_threads);
    std::vector<sieve_timings> duration_sieve_uint128_t(max_threads);
    std::vector<sieve_timings> duration_sieve_mpz_c(max_threads);
    std::vector<sieve_timings> duration_sieve_uint64_t_bulk(max_threads);
    std::vector<sieve_timings> duration_sieve_uint128_t_bulk(max_threads);
    std::vector<sieve_timings> duration_sieve_mpz_c_bulk(max_threads);
    std::cout << "Benchmarking sieve iteration..." << std::flush;
    for (size_t i = 0; i < max_threads; i++) {
        omp_set_num_threads(i + 1);
        duration_sieve_uint64_t[i] = sieve_iteration_time<uint64_t>(sieve_tree_levels, SieveOptions{}, iterations, 1);
        duration_sieve_uint128_t[i] = sieve_iteration_time<uint128_t>(sieve_tree_levels, SieveOptions{}, iterations, 1);
        duration_sieve_mpz_c[i] = sieve_iteration_time<mpz_class>(sieve_tree_levels, SieveOptions{}, iterations, 1);
        duration_sieve_uint64_t_bulk[i] = sieve_iteration_time<uint64_t>(sieve_tree_levels, SieveOptions{}, iterations, batch_size);
        duration_sieve_uint128_t_bulk[i] = sieve_iteration_time<uint128_t>(sieve_tree_levels, SieveOptions{}, iterations, batch_size);
        duration_sieve_mpz_c_bulk[i] = sieve_iteration_time<mpz_class>(sieve_tree_levels, SieveOptions{}, iterations, batch_size);
    }
    std::cout << " done." << std::endl << std::endl;

    // Print Table
    std::vector<std::vector<std::string>> table;
    table.push_back({"Item", "uint64_t", "uint128_t", "mpz_class", "Unit", "u128 Delta", "u128 Delta (+%)", "MPZ Delta", "MPZ Delta (+%)"});
    table.push_back(add("Base Data Type", sizeof(uint64_t), sizeof(uint128_t), sizeof(mpz_class)));

    // Collatz
    table.push_back({""});
    table.push_back(add("Collatz (shallow)", sizeof(Collatz<uint64_t>), sizeof(Collatz<uint128_t>), sizeof(Collatz<mpz_class>)));
    table.push_back({"Collatz(27) (deep)", "--", "--", "--", "--", "--", "--", "--", "--"});
    table.push_back(add("  With 112 Stops, With Metadata", collatz_with_seq_and_metadata_uint64_t.deep_size(), collatz_with_seq_and_metadata_uint128_t.deep_size(), collatz_with_seq_and_metadata_mpz_c.deep_size()));
    table.push_back(add("  With 112 Stops, Without Metadata", collatz_with_seq_no_metadata_uint64_t.deep_size(), collatz_with_seq_no_metadata_uint128_t.deep_size(), collatz_with_seq_no_metadata_mpz_c.deep_size()));
    table.push_back(add("  Without Stops, With Metadata", collatz_with_no_seq_metadata_uint64_t.deep_size(), collatz_with_no_seq_metadata_uint128_t.deep_size(), collatz_with_no_seq_metadata_mpz_c.deep_size()));

    // Node
    table.push_back({""});
    table.push_back(add("Node (shallow)", sizeof(Node<uint64_t>), sizeof(Node<uint128_t>), sizeof(Node<mpz_class>)));
    table.push_back(add("Node(27) (deep)", node_uint64_t.deep_size(), node_uint128_t.deep_size(), node_mpz_c.deep_size()));

    // BinaryTree
    table.push_back({""});
    table.push_back(add("BinaryTree (shallow)", sizeof(NodeBitmap<uint64_t>), sizeof(NodeBitmap<uint128_t>), sizeof(NodeBitmap<mpz_class>)));
    table.push_back({std::format("BinaryTree (deep, {} levels)", levels).c_str(), "--", "--", "--", "--", "--", "--", "--", "--"});
    table.push_back(add("  Materialized, Without Pruning", tree_uint64_t_without_pruning.deep_size(), tree_uint128_t_without_pruning.deep_size(), tree_mpz_c_without_pruning.deep_size()));
    table.push_back(add("    As Megabytes", tree_uint64_t_without_pruning.deep_size()/1024/1024, tree_uint128_t_without_pruning.deep_size()/1024/1024, tree_mpz_c_without_pruning.deep_size()/1024/1024, "Mbytes"));
    table.push_back(add("    As Gigabytes", tree_uint64_t_without_pruning.deep_size()/1024/1024/1024, tree_uint128_t_without_pruning.deep_size()/1024/1024/1024, tree_mpz_c_without_pruning.deep_size()/1024/1024/1024, "Gbytes"));
    table.push_back(add("    According to RSS", rss_uint64_t, rss_uint128_t, rss_mpz_c));
    table.push_back(add("    Nodes", tree_uint64_t_without_pruning.node_count(), tree_uint128_t_without_pruning.node_count(), tree_mpz_c_without_pruning.node_count().get_ui(), "nodes"));
    table.push_back(add("      Bytes per Node (Internal Tracking)", bytes_per_node_uint64_t_without_pruning, bytes_per_node_uint128_t_without_pruning, bytes_per_node_mpz_c_without_pruning));
    table.push_back(add("      Bytes per Node (RSS Usage)", rss_bytes_per_node_uint64_t_without_pruning, rss_bytes_per_node_uint128_t_without_pruning, rss_bytes_per_node_mpz_c_without_pruning));
    table.push_back(add("      Difference: RSS - Internal", rss_bytes_per_node_uint64_t_without_pruning - bytes_per_node_uint64_t_without_pruning, rss_bytes_per_node_uint128_t_without_pruning - bytes_per_node_uint128_t_without_pruning, rss_bytes_per_node_mpz_c_without_pruning - bytes_per_node_mpz_c_without_pruning));
    table.push_back(add("  Materialized, With Pruning", tree_uint64_t_with_pruning.deep_size(), tree_uint128_t_with_pruning.deep_size(), tree_mpz_c_with_pruning.deep_size()));
    table.push_back(add("    As Megabytes", tree_uint64_t_with_pruning.deep_size()/1024/1024, tree_uint128_t_with_pruning.deep_size()/1024/1024, tree_mpz_c_with_pruning.deep_size()/1024/1024, "Mbytes"));
    table.push_back(add("    As Gigabytes", tree_uint64_t_with_pruning.deep_size()/1024/1024/1024, tree_uint128_t_with_pruning.deep_size()/1024/1024/1024, tree_mpz_c_with_pruning.deep_size()/1024/1024/1024, "Gbytes"));
    table.push_back(add("    According to RSS", rss_uint64_t_with_pruning, rss_uint128_t_with_pruning, rss_mpz_c_with_pruning));
    table.push_back(add("    Nodes", tree_uint64_t_with_pruning.node_count(), tree_uint128_t_with_pruning.node_count(), tree_mpz_c_with_pruning.node_count().get_ui(), "nodes"));
    table.push_back(add("      Bytes per Node (Internal Tracking)", bytes_per_node_uint64_t_with_pruning, bytes_per_node_uint128_t_with_pruning, bytes_per_node_mpz_c_with_pruning));
    table.push_back(add("      Bytes per Node (RSS Usage)", rss_bytes_per_node_uint64_t_with_pruning, rss_bytes_per_node_uint128_t_with_pruning, rss_bytes_per_node_mpz_c_with_pruning));
    table.push_back(add("      Difference: RSS - Internal", rss_bytes_per_node_uint64_t_with_pruning - bytes_per_node_uint64_t_with_pruning, rss_bytes_per_node_uint128_t_with_pruning - bytes_per_node_uint128_t_with_pruning, rss_bytes_per_node_mpz_c_with_pruning - bytes_per_node_mpz_c_with_pruning));
    table.push_back(add("  Implicit", tree_uint64_t_implicit.deep_size(), tree_uint128_t_implicit.deep_size(), tree_mpz_c_implicit.deep_size()));
    table.push_back(add("    As Megabytes", tree_uint64_t_implicit.deep_size()/1024/1024, tree_uint128_t_implicit.deep_size()/1024/1024, tree_mpz_c_implicit.deep_size()/1024/1024, "Mbytes"));
    table.push_back(add("    As Gigabytes", tree_uint64_t_implicit.deep_size()/1024/1024/1024, tree_uint128_t_implicit.deep_size()/1024/1024/1024, tree_mpz_c_implicit.deep_size()/1024/1024/1024, "Gbytes"));
    table.push_back(add("    According to RSS", rss_uint64_t_implicit, rss_uint128_t_implicit, rss_mpz_c_implicit));
    table.push_back(add("    Nodes", tree_uint64_t_implicit.node_count(), tree_uint128_t_implicit.node_count(), tree_mpz_c_implicit.node_count().get_ui(), "nodes"));
    table.push_back(add("      Bytes per Node (Internal Tracking)", bytes_per_node_uint64_t_implicit, bytes_per_node_uint128_t_implicit, bytes_per_node_mpz_c_implicit));
    table.push_back(add("      Bytes per Node (RSS Usage)", rss_bytes_per_node_uint64_t_implicit, rss_bytes_per_node_uint128_t_implicit, rss_bytes_per_node_mpz_c_implicit));
    table.push_back(add("      Difference: RSS - Internal", rss_bytes_per_node_uint64_t_implicit - bytes_per_node_uint64_t_implicit, rss_bytes_per_node_uint128_t_implicit - bytes_per_node_uint128_t_implicit, rss_bytes_per_node_mpz_c_implicit - bytes_per_node_mpz_c_implicit));

    // Compare tree sizes
    // This is tricky because we need floats.
    float u64ratio_unpruned_vs_pruned = 100.0f * tree_uint64_t_without_pruning.deep_size() / tree_uint64_t_with_pruning.deep_size();
    float u128_ratio_unpruned_vs_pruned = 100.0f * tree_uint128_t_without_pruning.deep_size() / tree_uint128_t_with_pruning.deep_size();
    float mpz_ratio_unpruned_vs_pruned = 100.0f * tree_mpz_c_without_pruning.deep_size() / tree_mpz_c_with_pruning.deep_size();
    float u64ratio_implicit_vs_pruned = 100.0f * tree_uint64_t_implicit.deep_size() / tree_uint64_t_with_pruning.deep_size();
    float u128_ratio_implicit_vs_pruned = 100.0f * tree_uint128_t_implicit.deep_size() / tree_uint128_t_with_pruning.deep_size();
    float mpz_ratio_implicit_vs_pruned = 100.0f * tree_mpz_c_implicit.deep_size() / tree_mpz_c_with_pruning.deep_size();
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

    // Comparison Build Speeds




    // Sieve Stuff
    table.push_back({""});
    table.push_back(add("Sieve (shallow)", sizeof(Sieve<uint64_t>), sizeof(Sieve<uint128_t>), sizeof(Sieve<mpz_class>)));
    table.push_back(add(std::format("Sieve (deep, {} levels)", sieve_tree_levels).c_str(), sieve_uint64_t.deep_size(), sieve_uint128_t.deep_size(), sieve_mpz_c.deep_size()));
    table.push_back(add("  According to RSS", rss_uint64_t_sieve, rss_uint128_t_sieve, rss_mpz_c_sieve));
    //
    // Sieve Rate Data
    table.push_back({"  Iterator Time", "--", "--", "--", "--", "--", "--", "--", "--"});
    for (size_t i = 0; i < max_threads; i++) {
        std::string name = "    1-By-1, " + std::to_string(i + 1) + " Threads";
        table.push_back(add(name.c_str(), duration_sieve_uint64_t[i].iterator.count(), duration_sieve_uint128_t[i].iterator.count(), duration_sieve_mpz_c[i].iterator.count(), "ms"));
        if (compare_threads) {
            for (size_t j = 0; j < i; j++) {
                std::string comparison = "      Vs. " + std::to_string(j + 1) + " threads";
                int delta_uint64_t = duration_sieve_uint64_t[i].iterator.count() - duration_sieve_uint64_t[j].iterator.count();
                int delta_uint128_t = duration_sieve_uint128_t[i].iterator.count() - duration_sieve_uint128_t[j].iterator.count();
                int delta_mpz_c = duration_sieve_mpz_c[i].iterator.count() - duration_sieve_mpz_c[j].iterator.count();
                table.push_back({comparison.c_str(), std::to_string(delta_uint64_t), std::to_string(delta_uint128_t), std::to_string(delta_mpz_c), "ms", "--", "--", "--", "--"});
            }
        }
    }
    for (size_t i = 0; i < max_threads; i++) {
        std::string name = "    Bulk, " + std::to_string(i + 1) + " Threads";
        table.push_back(add(name.c_str(), duration_sieve_uint64_t_bulk[i].iterator.count(), duration_sieve_uint128_t_bulk[i].iterator.count(), duration_sieve_mpz_c_bulk[i].iterator.count(), "ms"));
        if (compare_threads) {
            for (size_t j = 0; j < i; j++) {
                std::string comparison = "      Vs. " + std::to_string(j + 1) + " threads";
                int delta_uint64_t = duration_sieve_uint64_t_bulk[i].iterator.count() - duration_sieve_uint64_t_bulk[j].iterator.count();
                int delta_uint128_t = duration_sieve_uint128_t_bulk[i].iterator.count() - duration_sieve_uint128_t_bulk[j].iterator.count();
                int delta_mpz_c = duration_sieve_mpz_c_bulk[i].iterator.count() - duration_sieve_mpz_c_bulk[j].iterator.count();
                table.push_back({comparison.c_str(), std::to_string(delta_uint64_t), std::to_string(delta_uint128_t), std::to_string(delta_mpz_c), "ms", "--", "--", "--", "--"});
            }
        }
    }
    //
    // Pool Refill Time
    table.push_back({"  Pool Refill Time", "--", "--", "--", "--", "--", "--", "--", "--"});
    for (size_t i = 0; i < max_threads; i++) {
        std::string name = "    1-By-1, " + std::to_string(i + 1) + " Threads";
        table.push_back(add(name.c_str(), duration_sieve_uint64_t[i].pool_refill.count() / 1000, duration_sieve_uint128_t[i].pool_refill.count() / 1000, duration_sieve_mpz_c[i].pool_refill.count() / 1000, "ms"));
        if (compare_threads) {
            for (size_t j = 0; j < i; j++) {
                std::string comparison = "      Vs. " + std::to_string(j + 1) + " threads";
                int delta_uint64_t = (duration_sieve_uint64_t[i].pool_refill.count() / 1000) - (duration_sieve_uint64_t[j].pool_refill.count() / 1000);
                int delta_uint128_t = (duration_sieve_uint128_t[i].pool_refill.count() / 1000) - (duration_sieve_uint128_t[j].pool_refill.count() / 1000);
                int delta_mpz_c = (duration_sieve_mpz_c[i].pool_refill.count() / 1000) - (duration_sieve_mpz_c[j].pool_refill.count() / 1000);
                table.push_back({comparison.c_str(), std::to_string(delta_uint64_t), std::to_string(delta_uint128_t), std::to_string(delta_mpz_c), "ms", "--", "--", "--", "--"});
            }
        }
    }
    for (size_t i = 0; i < max_threads; i++) {
        std::string name = "    Bulk, " + std::to_string(i + 1) + " Threads";
        table.push_back(add(name.c_str(), duration_sieve_uint64_t_bulk[i].pool_refill.count() / 1000, duration_sieve_uint128_t_bulk[i].pool_refill.count() / 1000, duration_sieve_mpz_c_bulk[i].pool_refill.count() / 1000, "ms"));
        if (compare_threads) {
            for (size_t j = 0; j < i; j++) {
                std::string comparison = "      Vs. " + std::to_string(j + 1) + " threads";
                int delta_uint64_t = (duration_sieve_uint64_t_bulk[i].pool_refill.count() / 1000) - (duration_sieve_uint64_t_bulk[j].pool_refill.count() / 1000);
                int delta_uint128_t = (duration_sieve_uint128_t_bulk[i].pool_refill.count() / 1000) - (duration_sieve_uint128_t_bulk[j].pool_refill.count() / 1000);
                int delta_mpz_c = (duration_sieve_mpz_c_bulk[i].pool_refill.count() / 1000) - (duration_sieve_mpz_c_bulk[j].pool_refill.count() / 1000);
                table.push_back({comparison.c_str(), std::to_string(delta_uint64_t), std::to_string(delta_uint128_t), std::to_string(delta_mpz_c), "ms", "--", "--", "--", "--"});
            }
        }
    }
    //
    // Pool Refill Count
    table.push_back({"  Pool Refills", "--", "--", "--", "--", "--", "--", "--", "--"});
    for (size_t i = 0; i < max_threads; i++) {
        std::string name = "    1-By-1, " + std::to_string(i + 1) + " Threads";
        table.push_back(add(name.c_str(), duration_sieve_uint64_t[i].refills, duration_sieve_uint128_t[i].refills, duration_sieve_mpz_c[i].refills, "refills"));
        if (compare_threads) {
            for (size_t j = 0; j < i; j++) {
                std::string comparison = "      Vs. " + std::to_string(j + 1) + " threads";
                int delta_uint64_t = duration_sieve_uint64_t[i].refills - duration_sieve_uint64_t[j].refills;
                int delta_uint128_t = duration_sieve_uint128_t[i].refills - duration_sieve_uint128_t[j].refills;
                int delta_mpz_c = duration_sieve_mpz_c[i].refills - duration_sieve_mpz_c[j].refills;
                table.push_back({comparison.c_str(), std::to_string(delta_uint64_t), std::to_string(delta_uint128_t), std::to_string(delta_mpz_c), "refills", "--", "--", "--", "--"});
            }
        }
    }
    for (size_t i = 0; i < max_threads; i++) {
        std::string name = "    Bulk, " + std::to_string(i + 1) + " Threads";
        table.push_back(add(name.c_str(), duration_sieve_uint64_t_bulk[i].refills, duration_sieve_uint128_t_bulk[i].refills, duration_sieve_mpz_c_bulk[i].refills, "refills"));
        if (compare_threads) {
            for (size_t j = 0; j < i; j++) {
                std::string comparison = "      Vs. " + std::to_string(j + 1) + " threads";
                int delta_uint64_t = duration_sieve_uint64_t_bulk[i].refills - duration_sieve_uint64_t_bulk[j].refills;
                int delta_uint128_t = duration_sieve_uint128_t_bulk[i].refills - duration_sieve_uint128_t_bulk[j].refills;
                int delta_mpz_c = duration_sieve_mpz_c_bulk[i].refills - duration_sieve_mpz_c_bulk[j].refills;
                table.push_back({comparison.c_str(), std::to_string(delta_uint64_t), std::to_string(delta_uint128_t), std::to_string(delta_mpz_c), "refills", "--", "--", "--", "--"});
            }
        }
    }
    //
    // Pool Refill Loops
    table.push_back({"  Pool Refill Loops", "--", "--", "--", "--", "--", "--", "--", "--"});
    for (size_t i = 0; i < max_threads; i++) {
        std::string name = "    1-By-1, " + std::to_string(i + 1) + " Threads";
        table.push_back(add(name.c_str(), duration_sieve_uint64_t[i].refill_fill_loops, duration_sieve_uint128_t[i].refill_fill_loops, duration_sieve_mpz_c[i].refill_fill_loops, "loops"));
        if (compare_threads) {
            for (size_t j = 0; j < i; j++) {
                std::string comparison = "      Vs. " + std::to_string(j + 1) + " threads";
                int delta_uint64_t = duration_sieve_uint64_t[i].refill_fill_loops - duration_sieve_uint64_t[j].refill_fill_loops;
                int delta_uint128_t = duration_sieve_uint128_t[i].refill_fill_loops - duration_sieve_uint128_t[j].refill_fill_loops;
                int delta_mpz_c = duration_sieve_mpz_c[i].refill_fill_loops - duration_sieve_mpz_c[j].refill_fill_loops;
                table.push_back({comparison.c_str(), std::to_string(delta_uint64_t), std::to_string(delta_uint128_t), std::to_string(delta_mpz_c), "loops", "--", "--", "--", "--"});
            }
        }
    }
    for (size_t i = 0; i < max_threads; i++) {
        std::string name = "    Bulk, " + std::to_string(i + 1) + " Threads";
        table.push_back(add(name.c_str(), duration_sieve_uint64_t_bulk[i].refill_fill_loops, duration_sieve_uint128_t_bulk[i].refill_fill_loops, duration_sieve_mpz_c_bulk[i].refill_fill_loops, "loops"));
        if (compare_threads) {
            for (size_t j = 0; j < i; j++) {
                std::string comparison = "      Vs. " + std::to_string(j + 1) + " threads";
                int delta_uint64_t = duration_sieve_uint64_t_bulk[i].refill_fill_loops - duration_sieve_uint64_t_bulk[j].refill_fill_loops;
                int delta_uint128_t = duration_sieve_uint128_t_bulk[i].refill_fill_loops - duration_sieve_uint128_t_bulk[j].refill_fill_loops;
                int delta_mpz_c = duration_sieve_mpz_c_bulk[i].refill_fill_loops - duration_sieve_mpz_c_bulk[j].refill_fill_loops;
                table.push_back({comparison.c_str(), std::to_string(delta_uint64_t), std::to_string(delta_uint128_t), std::to_string(delta_mpz_c), "loops", "--", "--", "--", "--"});
            }
        }
    }
    //
    // Pool Premature Refills
    table.push_back({"  Pool Premature Refills", "--", "--", "--", "--", "--", "--", "--", "--"});
    for (size_t i = 0; i < max_threads; i++) {
        std::string name = "    1-By-1, " + std::to_string(i + 1) + " Threads";
        table.push_back(add(name.c_str(), duration_sieve_uint64_t[i].premature_refills, duration_sieve_uint128_t[i].premature_refills, duration_sieve_mpz_c[i].premature_refills, "loops"));
        if (compare_threads) {
            for (size_t j = 0; j < i; j++) {
                std::string comparison = "      Vs. " + std::to_string(j + 1) + " threads";
                int delta_uint64_t = duration_sieve_uint64_t[i].premature_refills - duration_sieve_uint64_t[j].premature_refills;
                int delta_uint128_t = duration_sieve_uint128_t[i].premature_refills - duration_sieve_uint128_t[j].premature_refills;
                int delta_mpz_c = duration_sieve_mpz_c[i].premature_refills - duration_sieve_mpz_c[j].premature_refills;
                table.push_back({comparison.c_str(), std::to_string(delta_uint64_t), std::to_string(delta_uint128_t), std::to_string(delta_mpz_c), "loops", "--", "--", "--", "--"});
            }
        }
    }
    for (size_t i = 0; i < max_threads; i++) {
        std::string name = "    Bulk, " + std::to_string(i + 1) + " Threads";
        table.push_back(add(name.c_str(), duration_sieve_uint64_t_bulk[i].premature_refills, duration_sieve_uint128_t_bulk[i].premature_refills, duration_sieve_mpz_c_bulk[i].premature_refills, "loops"));
        if (compare_threads) {
            for (size_t j = 0; j < i; j++) {
                std::string comparison = "      Vs. " + std::to_string(j + 1) + " threads";
                int delta_uint64_t = duration_sieve_uint64_t_bulk[i].premature_refills - duration_sieve_uint64_t_bulk[j].premature_refills;
                int delta_uint128_t = duration_sieve_uint128_t_bulk[i].premature_refills - duration_sieve_uint128_t_bulk[j].premature_refills;
                int delta_mpz_c = duration_sieve_mpz_c_bulk[i].premature_refills - duration_sieve_mpz_c_bulk[j].premature_refills;
                table.push_back({comparison.c_str(), std::to_string(delta_uint64_t), std::to_string(delta_uint128_t), std::to_string(delta_mpz_c), "loops", "--", "--", "--", "--"});
            }
        }
    }

    // Print it.
    printTable(table);

    // All done.
    return 0;
}
