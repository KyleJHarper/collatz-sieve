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
std::vector<std::string> add(const char *name, int u_version, int z_version, const char* unit="bytes") {
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
std::chrono::duration<double, std::milli> tree_build_time(size_t levels) {
    auto start = std::chrono::high_resolution_clock::now();
    BinaryTree tree = BinaryTree<T>(levels);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    return duration;
}

int main(int argc, char **argv) {
    size_t levels = 16;
    if (argc > 1) {
        levels = std::atoll(argv[1]);
    }

    // Size Data
    std::cout << "Building objects for size data with " << levels << " levels..." << std::flush;
    Collatz collatz_uint64_t = Collatz<uint64_t>(27);
    Collatz collatz_mpz_c = Collatz<mpz_class>(27);
    Node node_uint64_t = Node<uint64_t>(27);
    Node node_mpz_c = Node<mpz_class>(27);
    Node<uint64_t>::enable_sequenes();
    Node<mpz_class>::enable_sequenes();
    Node node_uint64_t_keep_collatz = Node<uint64_t>(27);
    Node node_mpz_c_keep_collatz = Node<mpz_class>(27);
    Node<uint64_t>::disable_sequenes();
    Node<mpz_class>::disable_sequenes();
    BinaryTree tree_uint64_t = BinaryTree<uint64_t>(levels);
    BinaryTree tree_mpz_c = BinaryTree<mpz_class>(levels);
    Node<uint64_t>::enable_sequenes();
    Node<mpz_class>::enable_sequenes();
    BinaryTree tree_uint64_t_keep_collatz = BinaryTree<uint64_t>(levels);
    BinaryTree tree_mpz_c_keep_collatz = BinaryTree<mpz_class>(levels);
    Node<uint64_t>::disable_sequenes();
    Node<mpz_class>::disable_sequenes();
    std::cout << " done." << std::endl;

    // Rate Data
    const size_t max_threads = 4;
    std::chrono::duration<double, std::milli> duration_uint64_t[max_threads];
    std::chrono::duration<double, std::milli> duration_mpz_class[max_threads];
    std::cout << "Benchmarking tree building..." << std::flush;
    for (size_t i = 1; i <= max_threads; i++) {
        omp_set_num_threads(i);
        duration_uint64_t[i - 1] = tree_build_time<uint64_t>(levels);
        duration_mpz_class[i - 1] = tree_build_time<mpz_class>(levels);
    }
    std::cout << " done." << std::endl << std::endl;

    // Print Table
    std::vector<std::vector<std::string>> table;
    table.push_back({"Item", "uint64_t", "mpz_class", "Unit", "Delta (abs)", "Delta (+%)"});
    table.push_back(add("Base Type", sizeof(uint64_t), sizeof(mpz_class)));
    table.push_back(add("Collatz (shallow)", sizeof(Collatz<uint64_t>), sizeof(Collatz<mpz_class>)));
    table.push_back(add("Collatz(27) (deep, 112 stops)", collatz_uint64_t.deep_size(), collatz_mpz_c.deep_size()));
    table.push_back(add("Node (shallow)", sizeof(Node<uint64_t>), sizeof(Node<mpz_class>)));
    table.push_back(add("Node(27) (deep, 112 stops)", node_uint64_t_keep_collatz.deep_size(), node_mpz_c_keep_collatz.deep_size()));
    table.push_back(add("Node(27) (deep, stops removed)", node_uint64_t.deep_size(), node_mpz_c.deep_size()));
    table.push_back(add("BinaryTree (shallow)", sizeof(BinaryTree<uint64_t>), sizeof(BinaryTree<mpz_class>)));
    // table.push_back(add("BinaryTree (deep, " + "16 levels, without stops)", tree_uint64_t.deep_size(), tree_mpz_c.deep_size()));
    table.push_back(add(std::format("BinaryTree (deep, {} levels, without stops)", levels).c_str(), tree_uint64_t.deep_size(), tree_mpz_c.deep_size()));
    table.push_back(add(std::format("BinaryTree (deep, {} levels, with stops)", levels).c_str(), tree_uint64_t_keep_collatz.deep_size(), tree_mpz_c_keep_collatz.deep_size()));
    // This is tricky because we need floats.
    float uint_ratio = 100.0f * tree_uint64_t_keep_collatz.deep_size() / tree_uint64_t.deep_size();
    float mpz_ratio = 100.0f * tree_mpz_c_keep_collatz.deep_size() / tree_mpz_c.deep_size();
    table.push_back({"BinaryTree Stops vs No Stops", std::to_string(uint_ratio), std::to_string(mpz_ratio), "%", "--", "--"});
    for (size_t i = 1; i <= max_threads; i++) {
        std::string name = "BinaryTree Build Time (" + std::to_string(i) + " threads)";
        table.push_back(add(name.c_str(), duration_uint64_t[i - 1].count(), duration_mpz_class[i - 1].count(), "ms"));
        for (size_t j = 1; j < i; j++) {
            std::string comparison = "  Vs. " + std::to_string(j) + " threads";
            int delta_uint64_t = duration_uint64_t[i - 1].count() - duration_uint64_t[j - 1].count();
            int delta_mpz_class = duration_mpz_class[i - 1].count() - duration_mpz_class[j - 1].count();
            table.push_back({comparison.c_str(), std::to_string(delta_uint64_t), std::to_string(delta_mpz_class), "ms", "--", "--"});
        }
    }
    printTable(table);

    return 0;
}
