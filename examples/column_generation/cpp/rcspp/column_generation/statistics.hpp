// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "rcspp/column_generation/master_problem.hpp"
#include "rcspp/column_generation/pricer.hpp"
#include "rcspp/column_generation/types.hpp"

namespace rcspp::column_generation {

/// @brief Which statistics a run collects.
///
/// The result itself is always filled, and so are the fields of @c IterationInfo that name no
/// switch. Each switch fills its fields of @c IterationInfo, empty when off.
struct StatisticsParams {
        /// @brief Time spent in LP solves and in pricing, over the run and at each iteration (two
        ///        clock reads per phase per iteration).
        bool timers = true;
        /// @brief What each pricer did at each iteration (@c IterationInfo::pricers): one clock
        ///        read per pricer, and the stages a pricer that has them ran.
        bool pricers = false;
        /// @brief The search engine's counts in each stage's details (extended labels, the
        ///        bidirectional search's half-way point and join); needs @c pricers.
        bool search = false;
        /// @brief The duals of each iteration (@c IterationInfo::duals): one copy per iteration.
        bool duals = false;
        /// @brief The nonzero values of each iteration's LP solution
        ///        (@c IterationInfo::master_values): one read of every column's value per
        ///        iteration.
        bool master_values = false;
        /// @brief The columns each iteration added (@c IterationInfo::added_columns): a copy of
        ///        each.
        bool columns = false;
};

/// @brief Statistics of one run: counters, and times when @c StatisticsParams::timers is set.
struct Statistics {
        /// @brief Columns added by the pricers during the run.
        size_t generated_columns = 0;
        /// @brief Artificial columns in the master.
        size_t artificial_columns = 0;
        /// @brief Cost of each artificial column (0 when none): an @c Infeasible run found no
        ///        solution at this cost.
        double artificial_cost = 0.0;
        /// @brief Wall-clock time of the run, in seconds.
        double total_seconds = 0.0;
        /// @brief Time spent in LP solves, in seconds (0 when timers are off).
        double relaxation_seconds = 0.0;
        /// @brief Time spent in pricing, in seconds (0 when timers are off).
        double pricing_seconds = 0.0;
};

/// @brief What one pricer did at one iteration, measured by the loop around its call.
struct PricerIterationInfo {
        /// @brief How its call ended.
        PricingStatus status = PricingStatus::Unknown;
        /// @brief Columns it returned.
        size_t new_columns = 0;
        /// @brief The most negative reduced cost of the columns it returned, its convexity dual
        ///        included; empty when it returned none.
        std::optional<double> min_reduced_cost;
        /// @brief Time of its call, in seconds.
        double seconds = 0.0;
        /// @brief Its own account of the call (@c PricingResult::search_details): for a
        ///        @c GraphPricer, the stages it ran; empty from a pricer without stages.
        std::vector<PricingStageInfo> search_details;
};

/// @brief What the read-only iteration hook sees.
struct IterationInfo {
        /// @brief Iteration number, from 1.
        size_t iteration = 0;
        /// @brief Value of the master LP at this iteration.
        double objective = 0.0;
        /// @brief The Lagrangian bound at this iteration's duals: a lower bound on the LP value
        ///        of the master with every column. -infinity when unknown: a pricer without a
        ///        bound (a heuristic stage, a search stopped early) or without a convexity row
        ///        that limits its paths.
        double lower_bound = -std::numeric_limits<double>::infinity();
        /// @brief Columns the pricers added at this iteration.
        size_t new_columns = 0;
        /// @brief Columns in the master after this iteration, artificial ones included.
        size_t total_columns = 0;
        /// @brief Time since the run started, in seconds.
        double elapsed_seconds = 0.0;
        /// @brief Time of this iteration's LP solve, in seconds (@c StatisticsParams::timers).
        std::optional<double> relaxation_seconds;
        /// @brief Time of this iteration's pricing, the columns' addition to the master
        ///        included, in seconds (@c StatisticsParams::timers).
        std::optional<double> pricing_seconds;
        /// @brief One entry per pricer, in the master's order (@c StatisticsParams::pricers).
        std::optional<std::vector<PricerIterationInfo>> pricers;
        /// @brief The LP's duals, one per row, as the pricers received them
        ///        (@c StatisticsParams::duals).
        std::optional<std::vector<double>> duals;
        /// @brief The LP solution's nonzero values: the columns' ids in the master solver (their
        ///        positions in @c MasterProblem::columns(), artificial ones included) and their
        ///        values, read before this iteration's columns were added
        ///        (@c StatisticsParams::master_values).
        std::optional<std::pair<std::vector<ColumnId>, std::vector<double>>> master_values;
        /// @brief The columns this iteration added, each with its id, pricer and solution
        ///        (@c StatisticsParams::columns).
        std::optional<std::vector<MasterColumn>> added_columns;
};

}  // namespace rcspp::column_generation
