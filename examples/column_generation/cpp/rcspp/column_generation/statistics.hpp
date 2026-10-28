// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>

namespace rcspp::column_generation {

/// @brief Which statistics a run collects. The result itself is always filled.
struct StatisticsParams {
        /// @brief Time spent in LP solves and in pricing (two clock reads per phase per
        ///        iteration).
        bool timers = true;
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

/// @brief What the read-only iteration hook sees.
struct IterationInfo {
        /// @brief Iteration number, from 1.
        size_t iteration = 0;
        /// @brief Value of the master LP at this iteration.
        double objective = 0.0;
        /// @brief Columns the pricers added at this iteration.
        size_t new_columns = 0;
        /// @brief Columns in the master after this iteration, artificial ones included.
        size_t total_columns = 0;
        /// @brief Time since the run started, in seconds.
        double elapsed_seconds = 0.0;
};

}  // namespace rcspp::column_generation
