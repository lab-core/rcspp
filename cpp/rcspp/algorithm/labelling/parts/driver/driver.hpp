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
/// It is built from the algorithm's params, which outlive it. @c begin is called at the start of
/// each solve (from @c Driven::initialize), once the search and its context are built and before
/// the start labels are: it may mask arcs (@c use_arc_mask), end each run at its first path
/// (@c end_runs_at_end_node), and returns whether to run at all. @c after_run(search,
/// number_of_runs) is called after each run, with the number of runs made so far, once the
/// solve's loop has recorded the labels left at the end nodes: it reads how the run ended
/// (@c last_run), may change the search, readies the next run from the start labels with
/// @c reseed, and returns whether to run again. The loop runs again only while labels wait and
/// no stop applies (the time limit, an interrupt, @c max_solutions reached).
///
/// What waits when the driver stops decides the solve's status: nothing gives @c Complete;
/// labels still waiting (those of a stopped run, or labels stored aside, or the start labels
/// after a @c reseed, or after @c begin refused) give @c Truncated, or the stop that applied. A
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

/// @brief How one run of a driven search ended (@c Driven::last_run), read by the driver's
///        @c after_run.
///
/// A run that neither recorded, nor was exhausted, nor ended with labels stored aside was
/// stopped by the time limit, an interrupt or the memory limit; the solve's status says which.
struct RunEnd {
        /// @brief Whether the run recorded an end-node label, in a step or, once the run was
        ///        over, from the labels left at the end nodes (@c record_remaining_solutions):
        ///        with @c end_runs_at_end_node, the label it ended at. Recorded even when it is
        ///        not kept as a solution (see @ref new_solution).
        bool recorded = false;
        /// @brief Whether the run ended with no label waiting and none stored aside: not stopped
        ///        (the time limit, an interrupt, the memory limit), not ended by a label it
        ///        recorded, and with nothing the per-node quota or memory pressure stored aside
        ///        left after its last phase.
        bool exhausted = false;
        /// @brief Whether the run ran out of labels to take after its last phase (@c max_phases)
        ///        while the per-node quota or memory pressure had stored some aside.
        ///
        /// Those labels are put back to wait. The run did not try every path, so if it found
        /// none, that does not show none is left. With @c end_runs_at_end_node such a run found
        /// no path (@c recorded is false); without it, it may have recorded some.
        bool ended_with_stored_aside = false;
        /// @brief Whether the run added a solution the solve did not have.
        bool new_solution = false;
        /// @brief The cost of the end-node label the run recorded last; infinite when none.
        double cost = std::numeric_limits<double>::infinity();
        /// @brief Its path's arc ids, in path order: recorded as a solution or not (a duplicate,
        ///        or not below the upper bound).
        std::vector<size_t> arc_ids;
};

}  // namespace rcspp::parts
