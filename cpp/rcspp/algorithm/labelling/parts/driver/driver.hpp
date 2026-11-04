// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <cstddef>
#include <limits>
#include <vector>

#include "rcspp/algorithm/algorithm_params.hpp"

namespace rcspp::parts {

/// @brief A driver: runs one search several times in one solve, changing it between runs.
///
/// It is built from the algorithm's params. @c begin is called at the start of each solve, once
/// the search is ready: it may mask arcs (@c use_arc_mask), end each run at its first path
/// (@c end_runs_at_end_node), and returns whether to run at all. @c after_run is called after
/// each run (@p number_of_runs counts them, from 1): it reads how the run ended (@c last_run), may
/// change the search, starts the next run with @c reseed, and returns whether to run again. A
/// driver that is not exact says so with @c lose_proof.
///
/// @tparam DriverType The driver.
/// @tparam SearchType The driven search (@c detail::Driven).
template <typename DriverType, typename SearchType>
concept Driver = std::constructible_from<DriverType, const AlgorithmBaseParams&> &&
                 requires(DriverType driver, SearchType& search, size_t number_of_runs) {
                     { driver.begin(search) } -> std::same_as<bool>;
                     { driver.after_run(search, number_of_runs) } -> std::same_as<bool>;
                 };

/// @brief How one run of a driven search ended.
struct RunEnd {
        /// @brief Whether the run recorded an end-node label: with @c end_runs_at_end_node, the
        ///        label it ended at.
        bool recorded = false;
        /// @brief Whether the run ended with no label waiting: not stopped by a budget, an
        ///        interrupt or the memory limit.
        bool exhausted = false;
        /// @brief Whether the run added a solution the solve did not have.
        bool new_solution = false;
        /// @brief The cost of the end-node label the run recorded last; infinite when none.
        double cost = std::numeric_limits<double>::infinity();
        /// @brief Its path's arc ids, in path order: recorded as a solution or not (a duplicate,
        ///        or not below the upper bound).
        std::vector<size_t> arc_ids;
};

}  // namespace rcspp::parts
