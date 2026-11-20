// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// A vehicle routing problem with time windows, read from a file in Solomon's format.

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace vrp {

/// @brief A customer, or the depot (number 0): where it is, what it asks for and when.
struct Customer {
        size_t number = 0;
        double x = 0.0;
        double y = 0.0;
        double demand = 0.0;
        double ready_time = 0.0;
        double due_time = 0.0;
        double service_time = 0.0;

        bool operator==(const Customer&) const = default;
};

/// @brief The vehicles and the customers, the depot first.
struct Instance {
        std::string name;
        int vehicles = 0;
        double capacity = 0.0;
        std::vector<Customer> customers;

        [[nodiscard]] const Customer& depot() const { return customers.front(); }
};

/// @brief The Euclidean distance between two customers, also the travel time.
[[nodiscard]] double distance(const Customer& first, const Customer& second);

/// @brief The file @p path, or a file of that name in the repository's `instances/`.
///
/// `C101_25`, `C101_25.txt` and `instances/C101_25.txt` all find the same file.
///
/// @throws std::runtime_error When neither exists.
[[nodiscard]] std::string find_instance(std::string_view path);

/// @brief Reads a Solomon file: a title line, the line with the number of vehicles and their
///        capacity, then one line of seven numbers per customer (number, x, y, demand, ready
///        time, due time, service time), the depot first. Other lines are headers.
///
/// The instance is named after the file (`C101_25`): its title line names the full instance.
///
/// @throws std::runtime_error When the file cannot be read, has no vehicle line or no customers,
///                            or its customers are not numbered 0, 1, 2, ...
[[nodiscard]] Instance read_solomon(const std::string& path);

}  // namespace vrp
