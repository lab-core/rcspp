// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "rcspp/utils/timer.hpp"

using rcspp::Timer;

/// @brief BKS (best-known integer solution) costs for Solomon's 100-customer VRPTW benchmark.
///
/// Values from Solomon (1987) and subsequent improvements in the literature.
/// The CG LP relaxation satisfies lp_cost ≤ bks for any correctly solved instance.
inline const std::unordered_map<std::string, double> kSolomonBKS = {
    // C1 series (tight time windows)
    {"C101", 827.3},
    {"C102", 827.3},
    {"C103", 826.3},
    {"C104", 822.9},
    {"C105", 827.3},
    {"C106", 827.3},
    {"C107", 827.3},
    {"C108", 827.3},
    {"C109", 827.3},
    // R1 series (random)
    {"R101", 1637.7},
    {"R102", 1466.6},
    {"R103", 1208.7},
    {"R104", 971.5},
    {"R105", 1355.3},
    {"R106", 1234.6},
    {"R107", 1064.6},
    {"R108", 932.1},
    {"R109", 1146.2},
    {"R110", 1068.0},
    {"R111", 1048.7},
    {"R112", 948.6},
    // RC1 series (random-clustered)
    {"RC101", 1619.8},
    {"RC102", 1457.4},
    {"RC103", 1258.0},
    {"RC104", 1132.3},
    {"RC105", 1513.7},
    {"RC106", 1372.7},
    {"RC107", 1207.8},
    {"RC108", 1114.2},
    // C2 series (wide time windows)
    {"C201", 589.1},
    {"C202", 589.1},
    {"C203", 591.17},
    {"C204", 590.6},
    {"C205", 588.88},
    {"C206", 588.49},
    {"C207", 588.29},
    {"C208", 586.4},
    // R2 series (random, wide time windows)
    {"R201", 1143.2},
    {"R202", 1029.6},
    {"R203", 870.8},
    {"R204", 731.4},
    {"R205", 954.4},
    {"R206", 906.1},
    {"R207", 890.6},
    {"R208", 726.8},
    {"R209", 855.3},
    {"R210", 939.4},
    {"R211", 885.7},
    // RC2 series (random-clustered, wide time windows)
    {"RC201", 1261.8},
    {"RC202", 1092.3},
    {"RC203", 923.7},
    {"RC204", 783.5},
    {"RC205", 1154.0},
    {"RC206", 1051.1},
    {"RC207", 962.9},
    {"RC208", 776.1},
};

/// @brief Tolerance above BKS before flagging a result as a regression.
constexpr double kBKSTolerance = 1e-3;

constexpr size_t kMetricWidth = 8;
constexpr size_t kCostWidth = 10;
constexpr size_t kGapWidth = 8;
constexpr size_t kColWidth = 12;

/// @brief Format and append one benchmark row (instance + cost + gap + per-algo times).
///
/// @param instance    Instance name shown in the first column.
/// @param lp_cost     Final CG LP objective; NaN disables the cost/gap columns.
/// @param timers      Per-algorithm elapsed times.
/// @param labels      Algorithm names (same length as timers).
/// @param col_widths  Pre-computed column widths for alignment.
/// @return            Formatted row string (no trailing separator).
inline std::string format_benchmark_row(const std::string& instance, double lp_cost,
                                        const std::vector<Timer>& timers,
                                        const std::vector<std::string>& labels,
                                        const std::vector<size_t>& col_widths) {
    std::ostringstream out;
    bool has_cost = std::isfinite(lp_cost);

    out << std::left << std::setw(static_cast<int>(kMetricWidth)) << instance << " ";

    if (has_cost) {
        // cost column
        std::ostringstream cost_ss;
        cost_ss << std::fixed << std::setprecision(2) << lp_cost;
        out << std::right << std::setw(static_cast<int>(kCostWidth)) << cost_ss.str() << " ";

        // gap% vs BKS
        auto it = kSolomonBKS.find(instance);
        if (it != kSolomonBKS.end()) {
            double bks = it->second;
            double gap = (lp_cost - bks) / bks * 100.0;
            std::ostringstream gap_ss;
            gap_ss << std::fixed << std::setprecision(2) << gap << "%";
            if (lp_cost > bks + kBKSTolerance) {
                gap_ss << " FAIL";
            }
            out << std::right << std::setw(static_cast<int>(kGapWidth)) << gap_ss.str() << " ";
        } else {
            out << std::right << std::setw(static_cast<int>(kGapWidth)) << "n/a" << " ";
        }
    } else {
        out << std::right << std::setw(static_cast<int>(kCostWidth)) << "-" << " ";
        out << std::right << std::setw(static_cast<int>(kGapWidth)) << "-" << " ";
    }

    for (size_t i = 0; i < timers.size(); ++i) {
        out << std::right << std::setw(static_cast<int>(col_widths[i]))
            << timers[i].elapsed_to_hms() << " ";
    }
    out << "\n";
    return out.str();
}

/// @brief Print a full benchmark table (header + one row per instance + total row).
///
/// @param rows        Pre-formatted data rows: {instance_name, lp_cost, timers}.
/// @param labels      Algorithm column headers.
/// @param total_timers Accumulated timers for the "Total" footer row.
/// @return            Full table as a string.
inline std::string format_benchmark_table(
    const std::vector<std::tuple<std::string, double, std::vector<Timer>>>& rows,
    const std::vector<std::string>& labels, const std::vector<Timer>& total_timers) {
    // Compute column widths from data
    std::vector<size_t> col_widths(labels.size(), kColWidth);
    for (size_t i = 0; i < labels.size(); ++i) {
        col_widths[i] = std::max(col_widths[i], labels[i].size());
        for (const auto& [name, cost, timers] : rows) {
            col_widths[i] = std::max(col_widths[i], timers[i].elapsed_to_hms().size());
        }
    }

    size_t total_w = kMetricWidth + 1 + kCostWidth + 1 + kGapWidth + 1;
    for (size_t w : col_widths) {
        total_w += w + 1;
    }

    std::ostringstream out;

    // Header
    out << std::left << std::setw(static_cast<int>(kMetricWidth)) << "Instance" << " ";
    out << std::right << std::setw(static_cast<int>(kCostWidth)) << "LP Cost" << " ";
    out << std::right << std::setw(static_cast<int>(kGapWidth)) << "Gap%" << " ";
    for (size_t i = 0; i < labels.size(); ++i) {
        out << std::right << std::setw(static_cast<int>(col_widths[i])) << labels[i] << " ";
    }
    out << "\n" << std::string(total_w, '-') << "\n";

    // Data rows
    for (const auto& [name, cost, timers] : rows) {
        out << format_benchmark_row(name, cost, timers, labels, col_widths);
    }

    // Total row
    out << std::string(total_w, '-') << "\n";
    out << format_benchmark_row("Total",
                                std::numeric_limits<double>::quiet_NaN(),
                                total_timers,
                                labels,
                                col_widths);

    return out.str();
}
