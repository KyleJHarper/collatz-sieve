#include <cstdint>
#include <gmpxx.h>
#include <iostream>
#include <iomanip>
#include "collatz/binary_tree.hpp"
#include "collatz/collatz.hpp"


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
int main() {
    // Size Data
    Collatz collatz_uint64_t = Collatz<uint64_t>(27);
    Collatz collatz_mpz_c = Collatz<mpz_class>(27);
    Node node_uint64_t = Node<uint64_t>(27);
    Node node_mpz_c = Node<mpz_class>(27);
    BinaryTree tree_uint64_t = BinaryTree<uint64_t>(16);
    BinaryTree tree_mpz_c = BinaryTree<mpz_class>(16);

    std::vector<std::vector<std::string>> table;
    table.push_back({"Item", "uint64_t", "mpz_class"});
    table.push_back({"Base Type", std::to_string(sizeof(uint64_t)), std::to_string(sizeof(mpz_class))});
    table.push_back({"Collatz (shallow)", std::to_string(sizeof(Collatz<uint64_t>)), std::to_string(sizeof(Collatz<mpz_class>))});
    table.push_back({"Collatz(27) (deep, 112 stops)", std::to_string(collatz_uint64_t.deep_size()), std::to_string(collatz_mpz_c.deep_size())});
    table.push_back({"Node (shallow)", std::to_string(sizeof(Node<uint64_t>)), std::to_string(sizeof(Node<mpz_class>))});
    table.push_back({"Node(27) (deep, 112 stops)", std::to_string(node_uint64_t.deep_size()), std::to_string(node_mpz_c.deep_size())});
    table.push_back({"BinaryTree (shallow)", std::to_string(sizeof(BinaryTree<uint64_t>)), std::to_string(sizeof(BinaryTree<mpz_class>))});
    table.push_back({"BinaryTree (deep, 16 levels)", std::to_string(tree_uint64_t.deep_size()), std::to_string(tree_mpz_c.deep_size())});

    printTable(table);
    return 0;
}
