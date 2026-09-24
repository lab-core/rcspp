// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "vrp/instance.hpp"
#include "vrp/instance_reader.hpp"

// The benchmarks' shared constants and instance-level helpers. Under util/ because both
// bidirectional benchmark TUs include it, and a test_*.hpp may be included by only one.

namespace bidirectional_benchmark {

constexpr double kTolerance = 1e-9;
constexpr double kOptimal = -319.87786809696524415;

/// @brief A runaway guard, not a measurement: four hours per solve. It must be high enough for
/// the slower forward reference to finish, or the comparison silently becomes a truncated one.
constexpr double kTimeoutSeconds = 14400.0;

/// @brief How attractive a customer is under the synthetic duals; see `synthetic_duals`.
constexpr double kDualAlpha = 1.0;

/// @brief A hard cap on process RSS, so a runaway solve stops rather than taking the machine down.
constexpr double kMemoryCapGiB = 8.0;

/// @brief The equal time budget each algorithm gets on the full-size instances, which do not
/// finish; the test compares how far each search gets.
constexpr double kBudgetSeconds = 15.0;

/// @brief The 75-customer instances both bidirectional benchmarks run, cheapest family first.
///
/// @return The instance names, without the `.txt`.
inline const std::vector<std::string>& seventy_five_customer_instances() {
    static const std::vector<std::string> names{
        // R1 -- short horizon, narrow windows.
        "R101_75",
        "R102_75",
        "R105_75",
        "R103_75",
        "R107_75",
        // C1 -- clustered, long horizon, narrow windows.
        "C101_75",
        "C102_75",
        "C105_75",
        "C103_75",
        // RC1 -- mixed geography, short horizon.
        "RC101_75",
        "RC103_75",
        "RC105_75",
        "RC102_75",
        // C2 -- the longest horizons in the set.
        "C201_75",
        "C203_75",
        "C205_75",
        "C202_75",
        // RC2.
        "RC201_75",
        // R2 -- long horizon, wide windows: the family that gains most, and the slowest.
        "R201_75"};
    return names;
}

/// @brief Synthetic duals for instances that have no dual files.
///
/// A customer's dual is @p alpha times its round-trip distance from the depot, so pricing favours
/// long routes. These are not LP duals; the costs are only meaningful for comparing algorithms
/// on the same input.
///
/// @param instance The instance whose customers to price.
/// @param alpha    Customer attractiveness; 1.0 makes a direct out-and-back trip break even.
/// @return The duals, by customer id.
inline std::map<size_t, double> synthetic_duals(const Instance& instance, double alpha) {
    const auto& depot = instance.get_depot_customer();
    std::map<size_t, double> duals;
    for (const auto& [customer_id, customer] : instance.get_customers_by_id()) {
        if (customer_id == depot.id) {
            duals[customer_id] = 0.0;
            continue;
        }
        const double dx = customer.pos_x - depot.pos_x;
        const double dy = customer.pos_y - depot.pos_y;
        duals[customer_id] = alpha * 2.0 * std::sqrt((dx * dx) + (dy * dy));
    }
    return duals;
}

/// @brief Reads one Solomon instance by name.
///
/// @param name The file stem under `instances/`.
/// @return The parsed instance.
inline Instance load(const std::string& name) {
    // 4, not 3 as in test_*.hpp: this header is one directory deeper. A wrong depth yields an
    // empty instance, not a missing-file error.
    const std::string root_dir = file_parent_dir(__FILE__, 4);
    InstanceReader reader(root_dir + "/instances/" + name + ".txt");
    return reader.read();
}

/// @brief Params for a bidirectional run whose clock is the time slot.
///
/// @param horizon   The depot's due time; `H` is half of it.
/// @param timeout_s The per-solve budget.
/// @return The parameters.
inline rcspp::AlgorithmBaseParams bidirectional_params(double horizon, double timeout_s) {
    rcspp::AlgorithmBaseParams params;
    params.critical_resource_index = 1;  // time
    params.half_way_point = horizon / 2.0;
    params.timeout_s = timeout_s;
    params.max_memory_gb = kMemoryCapGiB;
    return params;
}

/// @brief Params for a forward reference run.
///
/// @param timeout_s The per-solve budget.
/// @return The parameters.
inline rcspp::AlgorithmBaseParams forward_params(double timeout_s) {
    rcspp::AlgorithmBaseParams params;
    params.timeout_s = timeout_s;
    params.max_memory_gb = kMemoryCapGiB;
    return params;
}

}  // namespace bidirectional_benchmark
