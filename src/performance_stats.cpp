#include <climits>
#include <cstdint>
#include <gmpxx.h>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>
#include <format>
#include "collatz/binary_tree.hpp"
#include "collatz/collatz.hpp"
#include "collatz/concepts.hpp"
#include <fstream>
#include <unistd.h>
#include "include/CLI11.hpp"


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
std::vector<std::string> add(const char *name, int64_t u_version, int64_t z_version, const char* unit="bytes") {
    std::vector<std::string> vector;
    int delta_abs = z_version - u_version;
    vector.push_back(name);
    vector.push_back(std::to_string(u_version));
    vector.push_back(std::to_string(z_version));
    vector.push_back(unit);
    vector.push_back(std::to_string(delta_abs));
    vector.push_back(std::to_string(100.0f * delta_abs / u_version) + '%');
    return vector;
}
template<IntegralOrMPZClass T>
std::chrono::duration<double, std::milli> tree_build_time(size_t levels, const BinaryTreeOptions& opts) {
    auto start = std::chrono::high_resolution_clock::now();
    BinaryTree tree = BinaryTree<T>(levels, opts);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    return duration;
}

int main(int argc, char **argv) {
    CLI::App options("Generates performance stats for internal diagnostics of our code.");
    size_t levels;
    bool compare_threads = false;
    options.add_option("-l,--levels", levels, "How many levels to check.")->default_val(16);
    options.add_flag("-c,--compare-threads", compare_threads, "Compare tree-building times thread-by-thread (show deltas).");
    CLI11_PARSE(options, argc, argv);

    // Size Data
    std::cout << "Building objects for size data with " << levels << " levels..." << std::flush;
    Collatz collatz_with_seq_and_metadata_uint64_t = Collatz<uint64_t>(27, true, true);
    Collatz collatz_with_seq_and_metadata_mpz_c = Collatz<mpz_class>(27, true, true);
    Collatz collatz_with_seq_no_metadata_uint64_t = Collatz<uint64_t>(27, true, false);
    Collatz collatz_with_seq_no_metadata_mpz_c = Collatz<mpz_class>(27, true, false);
    Collatz collatz_with_no_seq_metadata_uint64_t = Collatz<uint64_t>(27, false, true);
    Collatz collatz_with_no_seq_metadata_mpz_c = Collatz<mpz_class>(27, false, true);
    Node node_uint64_t = Node<uint64_t>(27, false);
    Node node_mpz_c = Node<mpz_class>(27, false);
    Node node_uint64_t_with_metadata = Node<uint64_t>(27, true);
    Node node_mpz_c_with_metadata = Node<mpz_class>(27, true);

    // Binrary Tree Option Sets
    BinaryTreeOptions BTWithoutPruneWithMetadata = {.track_node_metadata = true, .prune_hwm_nodes = false };
    BinaryTreeOptions BTWithoutPruneWithoutMetadata = {.track_node_metadata = false, .prune_hwm_nodes = false };
    BinaryTreeOptions BTWithPruneWithoutMetadata = {.track_node_metadata = false, .prune_hwm_nodes = true, .prune_parent_levels = true};

    // BT no metadata, no pruning
    size_t rss_t1 = getCurrentRSSBytes();
    BinaryTree tree_uint64_t = BinaryTree<uint64_t>(levels, BTWithoutPruneWithoutMetadata);
    size_t rss_uint64_t = getCurrentRSSBytes() - rss_t1;
    rss_t1 = getCurrentRSSBytes();
    BinaryTree tree_mpz_c = BinaryTree<mpz_class>(levels, BTWithoutPruneWithoutMetadata);
    size_t rss_mpz_c = getCurrentRSSBytes() - rss_t1;

    // BT yes metadata, no pruning
    rss_t1 = getCurrentRSSBytes();
    BinaryTree tree_uint64_t_with_metadata = BinaryTree<uint64_t>(levels, BTWithoutPruneWithMetadata);
    size_t rss_uint64_t_with_metadata = getCurrentRSSBytes() - rss_t1;
    rss_t1 = getCurrentRSSBytes();
    BinaryTree tree_mpz_c_with_metadata = BinaryTree<mpz_class>(levels, BTWithoutPruneWithMetadata);
    size_t rss_mpz_c_with_metadata = getCurrentRSSBytes() - rss_t1;

    // BT no metadata, yes pruning
    rss_t1 = getCurrentRSSBytes();
    BinaryTree tree_uint64_t_with_pruning = BinaryTree<uint64_t>(levels, BTWithPruneWithoutMetadata);
    size_t rss_uint64_t_with_pruning = getCurrentRSSBytes() - rss_t1;
    rss_t1 = getCurrentRSSBytes();
    BinaryTree tree_mpz_c_with_pruning = BinaryTree<mpz_class>(levels, BTWithPruneWithoutMetadata);
    size_t rss_mpz_c_with_pruning = getCurrentRSSBytes() - rss_t1;

    // Bytes Per Node
    size_t bytes_per_node_uint64_t = tree_uint64_t.deep_size() / tree_uint64_t.node_count();
    size_t bytes_per_node_mpz_c = tree_mpz_c.deep_size() / tree_mpz_c.node_count().get_ui();
    size_t bytes_per_node_uint64_t_with_metadata = tree_uint64_t_with_metadata.deep_size() / tree_uint64_t_with_metadata.node_count();
    size_t bytes_per_node_mpz_c_with_metadata = tree_mpz_c_with_metadata.deep_size() / tree_mpz_c_with_metadata.node_count().get_ui();
    size_t bytes_per_node_uint64_t_with_pruning = tree_uint64_t_with_pruning.deep_size() / tree_uint64_t_with_pruning.node_count();
    size_t bytes_per_node_mpz_c_with_pruning = tree_mpz_c_with_pruning.deep_size() / tree_mpz_c_with_pruning.node_count().get_ui();

    // RSS
    size_t rss_bytes_per_node_uint64_t = rss_uint64_t / tree_uint64_t.node_count();
    size_t rss_bytes_per_node_mpz_c = rss_mpz_c / tree_mpz_c.node_count().get_ui();
    size_t rss_bytes_per_node_uint64_t_with_metadata = rss_uint64_t_with_metadata / tree_uint64_t_with_metadata.node_count();
    size_t rss_bytes_per_node_mpz_c_with_metadata = rss_mpz_c_with_metadata / tree_mpz_c_with_metadata.node_count().get_ui();
    size_t rss_bytes_per_node_uint64_t_with_pruning = rss_uint64_t_with_pruning / tree_uint64_t_with_pruning.node_count();
    size_t rss_bytes_per_node_mpz_c_with_pruning = rss_mpz_c_with_pruning / tree_mpz_c_with_pruning.node_count().get_ui();
    std::cout << " done." << std::endl;

    // Rate Data
    const size_t max_threads = 4;
    std::chrono::duration<double, std::milli> duration_uint64_t[max_threads];
    std::chrono::duration<double, std::milli> duration_uint64_t_with_pruning[max_threads];
    std::chrono::duration<double, std::milli> duration_mpz_class[max_threads];
    std::chrono::duration<double, std::milli> duration_mpz_class_with_pruning[max_threads];
    std::cout << "Benchmarking tree building..." << std::flush;
    for (size_t i = 1; i <= max_threads; i++) {
        omp_set_num_threads(i);
        duration_uint64_t[i - 1] = tree_build_time<uint64_t>(levels, BTWithoutPruneWithoutMetadata);
        duration_uint64_t_with_pruning[i - 1] = tree_build_time<uint64_t>(levels, BTWithPruneWithoutMetadata);
        duration_mpz_class[i - 1] = tree_build_time<mpz_class>(levels, BTWithoutPruneWithoutMetadata);
        duration_mpz_class_with_pruning[i - 1] = tree_build_time<mpz_class>(levels, BTWithPruneWithoutMetadata);
    }
    std::cout << " done." << std::endl << std::endl;

    // Print Table
    std::vector<std::vector<std::string>> table;
    table.push_back({"Item", "uint64_t", "mpz_class", "Unit", "Delta (abs)", "Delta (+%)"});
    table.push_back(add("Base Type", sizeof(uint64_t), sizeof(mpz_class)));
    table.push_back(add("Collatz (shallow)", sizeof(Collatz<uint64_t>), sizeof(Collatz<mpz_class>)));
    table.push_back(add("Collatz(27) (deep, 112 stops, with metadata)", collatz_with_seq_and_metadata_uint64_t.deep_size(), collatz_with_seq_and_metadata_mpz_c.deep_size()));
    table.push_back(add("Collatz(27) (deep, 112 stops, without metadata)", collatz_with_seq_no_metadata_uint64_t.deep_size(), collatz_with_seq_no_metadata_mpz_c.deep_size()));
    table.push_back(add("Collatz(27) (deep, no stops, with metadata)", collatz_with_no_seq_metadata_uint64_t.deep_size(), collatz_with_no_seq_metadata_mpz_c.deep_size()));
    table.push_back(add("Node (shallow)", sizeof(Node<uint64_t>), sizeof(Node<mpz_class>)));
    table.push_back(add("Node(27) (deep, with metadata)", node_uint64_t_with_metadata.deep_size(), node_mpz_c_with_metadata.deep_size()));
    table.push_back(add("Node(27) (deep, without metadata)", node_uint64_t.deep_size(), node_mpz_c.deep_size()));
    table.push_back(add("BinaryTree (shallow)", sizeof(BinaryTree<uint64_t>), sizeof(BinaryTree<mpz_class>)));
    table.push_back(add(std::format("BinaryTree (deep, {} levels, without metadata)", levels).c_str(), tree_uint64_t.deep_size(), tree_mpz_c.deep_size()));
    table.push_back(add("  As Megabytes", tree_uint64_t.deep_size()/1024/1024, tree_mpz_c.deep_size()/1024/1024, "Mbytes"));
    table.push_back(add("  As Gigabytes", tree_uint64_t.deep_size()/1024/1024/1024, tree_mpz_c.deep_size()/1024/1024/1024, "Gbytes"));
    table.push_back(add("  According to RSS", rss_uint64_t, rss_mpz_c));
    table.push_back(add("  Nodes", tree_uint64_t.node_count(), tree_mpz_c.node_count().get_ui(), "nodes"));
    table.push_back(add("    Bytes per Node (Internal Tracking)", bytes_per_node_uint64_t, bytes_per_node_mpz_c));
    table.push_back(add("    Bytes per Node (RSS Usage)", rss_bytes_per_node_uint64_t, rss_bytes_per_node_mpz_c));
    table.push_back(add("    Difference: RSS - Internal", rss_bytes_per_node_uint64_t - bytes_per_node_uint64_t, rss_bytes_per_node_mpz_c - bytes_per_node_mpz_c));
    table.push_back(add(std::format("BinaryTree (deep, {} levels, with metadata)", levels).c_str(), tree_uint64_t_with_metadata.deep_size(), tree_mpz_c_with_metadata.deep_size()));
    table.push_back(add("  As Megabytes", tree_uint64_t_with_metadata.deep_size()/1024/1024, tree_mpz_c_with_metadata.deep_size()/1024/1024, "Mbytes"));
    table.push_back(add("  As Gigabytes", tree_uint64_t_with_metadata.deep_size()/1024/1024/1024, tree_mpz_c_with_metadata.deep_size()/1024/1024/1024, "Gbytes"));
    table.push_back(add("  According to RSS", rss_uint64_t_with_metadata, rss_mpz_c_with_metadata));
    table.push_back(add("  Nodes", tree_uint64_t_with_metadata.node_count(), tree_mpz_c_with_metadata.node_count().get_ui(), "nodes"));
    table.push_back(add("    Bytes per Node (Internal Tracking)", bytes_per_node_uint64_t_with_metadata, bytes_per_node_mpz_c_with_metadata));
    table.push_back(add("    Bytes per Node (RSS Usage)", rss_bytes_per_node_uint64_t_with_metadata, rss_bytes_per_node_mpz_c_with_metadata));
    table.push_back(add("    Difference: RSS - Internal", rss_bytes_per_node_uint64_t_with_metadata - bytes_per_node_uint64_t_with_metadata, rss_bytes_per_node_mpz_c_with_metadata - bytes_per_node_mpz_c_with_metadata));
    table.push_back(add(std::format("BinaryTree (deep, {} levels, with all pruning)", levels).c_str(), tree_uint64_t_with_pruning.deep_size(), tree_mpz_c_with_pruning.deep_size()));
    table.push_back(add("  As Megabytes", tree_uint64_t_with_pruning.deep_size()/1024/1024, tree_mpz_c_with_pruning.deep_size()/1024/1024, "Mbytes"));
    table.push_back(add("  As Gigabytes", tree_uint64_t_with_pruning.deep_size()/1024/1024/1024, tree_mpz_c_with_pruning.deep_size()/1024/1024/1024, "Gbytes"));
    table.push_back(add("  According to RSS", rss_uint64_t_with_pruning, rss_mpz_c_with_pruning));
    table.push_back(add("  Nodes", tree_uint64_t_with_pruning.node_count(), tree_mpz_c_with_pruning.node_count().get_ui(), "nodes"));
    table.push_back(add("    Bytes per Node (Internal Tracking)", bytes_per_node_uint64_t_with_pruning, bytes_per_node_mpz_c_with_pruning));
    table.push_back(add("    Bytes per Node (RSS Usage)", rss_bytes_per_node_uint64_t_with_pruning, rss_bytes_per_node_mpz_c_with_pruning));
    table.push_back(add("    Difference: RSS - Internal", rss_bytes_per_node_uint64_t_with_pruning - bytes_per_node_uint64_t_with_pruning, rss_bytes_per_node_mpz_c_with_pruning - bytes_per_node_mpz_c_with_pruning));

    // This is tricky because we need floats.
    float uint_ratio = 100.0f * tree_uint64_t_with_metadata.deep_size() / tree_uint64_t.deep_size();
    float mpz_ratio = 100.0f * tree_mpz_c_with_metadata.deep_size() / tree_mpz_c.deep_size();
    table.push_back({"BinaryTree Metadata vs No Metadata", std::to_string(uint_ratio), std::to_string(mpz_ratio), "%", "--", "--"});

    // Now rate data.
    for (size_t i = 1; i <= max_threads; i++) {
        std::string name = "BinaryTree Build Time (no pruning, " + std::to_string(i) + " threads)";
        table.push_back(add(name.c_str(), duration_uint64_t[i - 1].count(), duration_mpz_class[i - 1].count(), "ms"));
        if (compare_threads) {
            for (size_t j = 1; j < i; j++) {
                std::string comparison = "  Vs. " + std::to_string(j) + " threads";
                int delta_uint64_t = duration_uint64_t[i - 1].count() - duration_uint64_t[j - 1].count();
                int delta_mpz_class = duration_mpz_class[i - 1].count() - duration_mpz_class[j - 1].count();
                table.push_back({comparison.c_str(), std::to_string(delta_uint64_t), std::to_string(delta_mpz_class), "ms", "--", "--"});
            }
        }
    }
    for (size_t i = 1; i <= max_threads; i++) {
        std::string name = "BinaryTree Build Time (with pruning, " + std::to_string(i) + " threads)";
        table.push_back(add(name.c_str(), duration_uint64_t_with_pruning[i - 1].count(), duration_mpz_class_with_pruning[i - 1].count(), "ms"));
        if (compare_threads) {
            for (size_t j = 1; j < i; j++) {
                std::string comparison = "  Vs. " + std::to_string(j) + " threads";
                int delta_uint64_t = duration_uint64_t_with_pruning[i - 1].count() - duration_uint64_t_with_pruning[j - 1].count();
                int delta_mpz_class = duration_mpz_class_with_pruning[i - 1].count() - duration_mpz_class_with_pruning[j - 1].count();
                table.push_back({comparison.c_str(), std::to_string(delta_uint64_t), std::to_string(delta_mpz_class), "ms", "--", "--"});
            }
        }
    }
    printTable(table);

    return 0;
}
