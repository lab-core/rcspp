// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Dynamic ng augmentation: how a column-generation driver grows ng neighbourhoods from the cycles
// in an LP solution. RouteOpt's rule (`findNGMemorySets`, `reduceNGSet`) restated from the
// published description, with its constants; see analysis/route_opt/routeopt-next-pr.md, P2.
//
// Deliberately free of Gurobi and of `VRP`: it is a function of the LP columns and the current
// table, so the test suite can check it without a solver.

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <span>
#include <vector>

#include "rcspp/resource/ng_neighborhoods.hpp"

/// @brief RouteOpt's augmentation constants, named.
struct NgGrowthParams {
        size_t max_columns_with_a_cycle = 200;  ///< MaxNumColsInNGAug
        size_t max_cycle_length = 8;            ///< CYCLE_SIZE: nodes strictly inside
        size_t max_ng_size = 16;                ///< MAX_NG_SIZE
        double tail_off = 0.5;                  ///< NGAugTailOff
        double soft_time_factor = 1.0;          ///< NGAugTimeSoftThresholdFactor
        double hard_time_factor = 2.0;          ///< NGAugTimeHardThresholdFactor
        size_t max_rounds = 50;                 ///< not in RouteOpt; a runaway guard
};

/// @brief A column of the LP solution: its value and its node sequence.
struct LpColumn {
        double value = 0.0;
        std::vector<size_t> nodes;
};

/// @brief The table grown from the cycles in an LP solution, and what was added.
struct NgGrowth {
        std::map<size_t, std::set<size_t>> neighborhoods;
        size_t members_added = 0;
        size_t cycles_used = 0;  ///< Cycles that added at least one member.
};

/// @brief Below this an LP value counts as zero: the column is not in the solution.
inline constexpr double kLpValueEpsilon = 1e-6;

/// @brief Grows @p current from the cycles in @p columns, RouteOpt's rule.
///
/// 1. Columns with a positive value, in decreasing value, keeping at most
///    `max_columns_with_a_cycle` of those that contain a cycle.
/// 2. Their cycles (`rcspp::find_cycles`), ordered by increasing `length()` -- the nodes strictly
///    inside. **Ties keep the order they were found in**: columns by decreasing value, then by
///    position in the column.
/// 3. Every cycle of the shortest length present is used, even one longer than `max_cycle_length`,
///    so there is always something to add while any cycle remains. Longer cycles are used while
///    `length() <= max_cycle_length`.
/// 4. For each cycle used, its repeated node joins the neighbourhood of every interior node, unless
///    that neighbourhood already has `max_ng_size` members, or already holds it.
///
/// A cycle whose repeated node, or an interior node whose neighbourhood, @p current does not name
/// is skipped there, so the result always passes `rcspp::check_ng_update` against @p current.
///
/// @param current The table pricing uses now.
/// @param columns The LP columns; taken by value because they are sorted.
/// @param params  The constants.
/// @return The grown table, and how much was added.
[[nodiscard]] inline NgGrowth grow_ng(const std::map<size_t, std::set<size_t>>& current,
                                      std::vector<LpColumn> columns, const NgGrowthParams& params) {
    std::ranges::stable_sort(columns, [](const LpColumn& lhs, const LpColumn& rhs) {
        return lhs.value > rhs.value;
    });

    std::vector<rcspp::NgCycle> cycles;
    size_t columns_with_a_cycle = 0;
    for (const auto& column : columns) {
        if (!(column.value > kLpValueEpsilon) ||
            columns_with_a_cycle >= params.max_columns_with_a_cycle) {
            break;
        }
        auto found = rcspp::find_cycles(std::span<const size_t>(column.nodes));
        if (found.empty()) {
            continue;
        }
        ++columns_with_a_cycle;
        cycles.insert(cycles.end(), found.begin(), found.end());
    }

    NgGrowth growth{.neighborhoods = current, .members_added = 0, .cycles_used = 0};
    if (cycles.empty()) {
        return growth;
    }
    std::ranges::stable_sort(cycles, [](const rcspp::NgCycle& lhs, const rcspp::NgCycle& rhs) {
        return lhs.length() < rhs.length();
    });

    const size_t shortest = cycles.front().length();
    for (const auto& cycle : cycles) {
        if (cycle.length() != shortest && cycle.length() > params.max_cycle_length) {
            break;  // sorted: every later cycle is at least as long
        }
        if (!current.contains(cycle.node)) {
            continue;
        }
        bool added = false;
        for (const size_t interior : cycle.interior) {
            auto it = growth.neighborhoods.find(interior);
            if (it == growth.neighborhoods.end() || it->second.contains(cycle.node) ||
                it->second.size() >= params.max_ng_size) {
                continue;
            }
            it->second.insert(cycle.node);
            ++growth.members_added;
            added = true;
        }
        if (added) {
            ++growth.cycles_used;
        }
    }
    return growth;
}

/// @brief RouteOpt's `reduceNGSet`: keeps in each neighbourhood only the nodes adjacent to it in
///        the support graph of @p columns, plus the node itself.
///
/// The support graph has an edge between consecutive nodes of every column with a positive value,
/// either way round. Only meaningful from a non-empty start. It can loosen the relaxation, so the
/// bound may fall, but no column becomes infeasible: a smaller memory forbids less.
///
/// @param current The table pricing uses now.
/// @param columns The LP columns.
/// @return The reduced table, with the same keys.
[[nodiscard]] inline std::map<size_t, std::set<size_t>> reduce_to_support(
    const std::map<size_t, std::set<size_t>>& current, const std::vector<LpColumn>& columns) {
    std::map<size_t, std::set<size_t>> adjacent;
    for (const auto& column : columns) {
        if (!(column.value > kLpValueEpsilon)) {
            continue;
        }
        for (size_t i = 0; i + 1 < column.nodes.size(); ++i) {
            adjacent[column.nodes[i]].insert(column.nodes[i + 1]);
            adjacent[column.nodes[i + 1]].insert(column.nodes[i]);
        }
    }

    std::map<size_t, std::set<size_t>> reduced;
    for (const auto& [node_id, members] : current) {
        std::set<size_t> kept;
        const auto it = adjacent.find(node_id);
        for (const size_t member : members) {
            if (member == node_id || (it != adjacent.end() && it->second.contains(member))) {
                kept.insert(member);
            }
        }
        reduced.emplace(node_id, std::move(kept));
    }
    return reduced;
}
