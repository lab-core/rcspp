// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <cstddef>
#include <limits>
#include <vector>

#include "rcspp/algorithm/algorithm_params.hpp"

namespace rcspp::parts {

/// @brief A driver runs the same search several times in one solve, and may change the search
///        between runs.
///
/// The tabu searches are built this way: after each run, their driver forbids the arcs of the
/// path the run found, so that the next run looks for a different path.
///
/// A driver is built from the algorithm's params (it may keep a reference to them: they outlive
/// it). It has two hooks, each given the search (a @c detail::Driven):
///
/// - @c begin(search) is called once at the start of each solve, before the first run and
///   before the start labels are created. It can set the search up, for example give it an arc
///   mask (@c use_arc_mask) or make each run stop at the first path it finds
///   (@c end_runs_at_end_node). It returns false to skip the runs altogether.
/// - @c after_run(search, number_of_runs) is called after each run, with the number of runs
///   made so far. By then, the paths the run reached are recorded. It can read how the run
///   ended (@c last_run), change the search, and restart it from the start labels (@c reseed).
///   It returns true to run again, false to stop.
///
/// A new run starts only if @c after_run returns true, some label is waiting, and no limit was
/// reached (time limit, interrupt, @c max_solutions).
///
/// The solve's status depends on what is left when the driver stops. If no label is waiting,
/// the status is @c Complete. If some are waiting, it is @c Truncated (or the limit that stopped
/// the solve). Labels are left waiting, for example, when a limit stopped the last run, when the
/// driver called @c reseed and then stopped, or when @c begin returned false.
///
/// A @c Complete status is not enough for a proof: a driver that can hide the best path (as
/// forbidding arcs does) must call @c lose_proof.
///
/// @tparam DriverType The driver.
/// @tparam SearchType The driven search (@c detail::Driven).
template <typename DriverType, typename SearchType>
concept Driver = std::constructible_from<DriverType, const AlgorithmBaseParams&> &&
                 requires(DriverType driver, SearchType& search, size_t number_of_runs) {
                     { driver.begin(search) } -> std::same_as<bool>;
                     { driver.after_run(search, number_of_runs) } -> std::same_as<bool>;
                 };

/// @brief How the last run of a driven search ended. The driver reads it in @c after_run,
///        through @c Driven::last_run, to decide what to do next.
///
/// If @ref recorded, @ref exhausted and @ref ended_with_stored_aside are all false, a limit
/// stopped the run (the time limit, an interrupt or the memory limit); the solve's status says
/// which.
struct RunEnd {
        /// @brief True if the run found a path, that is, a label that reached an end node.
        ///
        /// The path may be found while the run extends labels, or among the labels left at the
        /// end nodes once the run is over (@c record_remaining_solutions). With
        /// @c end_runs_at_end_node, it is the path the run stopped at. It counts even if it was
        /// not kept as a solution; @ref new_solution says whether it was.
        bool recorded = false;
        /// @brief True if the run ended because it had nothing left to extend: no label was
        ///        waiting, and none was stored aside.
        ///
        /// It is false when a limit stopped the run, when the run stopped at a path it found
        /// (@c end_runs_at_end_node, @c max_solutions), or when labels stored aside were left
        /// after its last phase (@ref ended_with_stored_aside).
        bool exhausted = false;
        /// @brief True if the run used its last phase (@c max_phases) while some labels were
        ///        still stored aside (by the per-node quota or by memory pressure).
        ///
        /// Those labels are put back in the waiting labels. Since they were never extended, the
        /// run did not try every path: if it found no path, there may still be one. With
        /// @c end_runs_at_end_node, such a run found no path (@ref recorded is false), since it
        /// would have stopped at the first one.
        bool ended_with_stored_aside = false;
        /// @brief Whether the run added a solution the solve did not have.
        bool new_solution = false;
        /// @brief The cost of the path the run found, or infinity if it found none.
        ///
        /// With @c end_runs_at_end_node, it is the path the run stopped at. Otherwise, if the run
        /// found several paths, it is the last one found while the run extended labels; if it
        /// found none then, it is the last one read from the end nodes once the run was over.
        double cost = std::numeric_limits<double>::infinity();
        /// @brief The ids of the arcs of that path (the one @ref cost describes), in the order the
        ///        path uses them.
        ///
        /// They are filled even when the path was not kept as a solution: because the solve
        /// already had it, or because it does not cost less than the upper bound.
        std::vector<size_t> arc_ids;
};

}  // namespace rcspp::parts
