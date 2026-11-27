// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <limits>

#include "rcspp/algorithm/algorithm_params.hpp"
#include "rcspp/algorithm/arc_mask.hpp"
#include "rcspp/algorithm/labelling/parts/driver/driver.hpp"
#include "rcspp/algorithm/labelling/parts/driver/tabu_episodes.hpp"
#include "rcspp/algorithm/limits.hpp"
#include "rcspp/algorithm/tabu/tabu_arcs.hpp"

namespace rcspp::parts {

/// @brief The driver of @c strategy::ImprovingTabu: a first dive with no arc tabu (the
///        @b construction), then up to @c max_iterations more dives, each called an
///        @b episode. After each dive, the arcs of the path found are made @b tabu (forbidden)
///        for a few episodes, so that the next dives look for other paths.
///
/// How many episodes an arc stays tabu is its @b tenure: @c tabu_tenure plus an adaptive
/// @b extra. This driver lengthens the extra while the episodes find no better path, and
/// shortens it when one does: a longer tenure forbids more arcs at once, which pushes the dives
/// further from the paths already found.
///
/// The construction's path gives the first best cost. Its arcs become tabu if it costs less
/// than the upper bound. If the construction finds no path, the solve ends.
///
/// After each episode:
/// - It found a path that costs less than the upper bound, and less than the best cost by more
///   than @c tolerance: this is an improvement. Its cost becomes the best cost, the count of
///   episodes without improvement goes back to 0, and the extra shrinks by one.
/// - It found another path: the count goes up by one.
/// - It found no path: the extra grows by one at once, and the count goes up by one. If no arc
///   is tabu, the solve ends instead, as in @ref TabuEpisodes.
/// In the first two cases, the path's arcs become tabu. When the count reaches
/// @c non_improving_episodes_before_growth, the extra grows by one more and the count goes back
/// to 0. Then every tabu arc has one episode less to stay tabu. A limit that stops a run ends
/// the solve, as in @ref TabuEpisodes.
///
/// Each episode still stops at its first path, whatever it costs: only the count reads the best
/// cost. @c max_iterations must be finite.
///
/// The construction does not count down the tenures, so its arcs stay tabu for their full
/// tenure. The arcs of a later episode stay tabu one episode less, since that episode already
/// counts down once (as in @ref TabuEpisodes).
class ImprovingTabuEpisodes {
    public:
        /// @brief A driver that reads @p params: the iteration budget, the tenure, its noise and
        ///        seed, the exempt nodes, the tolerance, and how many episodes without an
        ///        improvement grow the tenure.
        ///
        /// @param params The algorithm's params; must outlive the driver.
        explicit ImprovingTabuEpisodes(const AlgorithmBaseParams& params)
            : params_(&params), tabu_(params, ArcMask::State::Soft) {}

        /// @brief Starts a solve: no arc tabu yet, no best cost, the search reading the tabu
        ///        arcs' mask, and each run ending at its first path.
        ///
        /// @param search The driven search.
        /// @return Whether to run: false when @c max_iterations is not finite (logged). With 0,
        ///         the construction alone runs.
        template <typename SearchType>
        bool begin(SearchType& search) {
            tabu_detail::begin(tabu_, search);
            search.end_runs_at_end_node();
            best_cost_ = std::numeric_limits<double>::infinity();
            number_of_non_improving_episodes_ = 0;
            if (params_->max_iterations >= kNoLimit) {
                LOG_ERROR(
                    "max_iterations must be set to a finite value for strategy::ImprovingTabu.\n");
                return false;
            }
            return true;
        }

        /// @brief Ends the construction or an episode: records the best cost, makes the path's
        ///        arcs tabu and adapts the tenure, or decides the solve is over; then reseeds the
        ///        next episode.
        ///
        /// @param search         The driven search.
        /// @param number_of_runs The runs so far, the construction included.
        /// @return Whether to run another episode.
        template <typename SearchType>
        bool after_run(SearchType& search, size_t number_of_runs) {
            const RunEnd& run = search.last_run();
            if (!run.recorded && !run.exhausted && !run.ended_with_stored_aside) {
                return false;  // stopped: the labels wait, and the status says why
            }
            if (number_of_runs == 1) {  // the construction
                if (!run.recorded) {
                    return false;  // the construction found no path; labels stored aside wait
                }
                best_cost_ = run.cost;
                if (best_cost_ < search.upper_bound()) {
                    tabu_detail::add(tabu_, search, run.arc_ids);
                }
            } else {
                if (!run.recorded) {
                    if (tabu_.list().empty()) {
                        // No path found, and no arc is tabu: the next episode would repeat this
                        // one, so the solve ends. Labels stored aside, if any, are left waiting.
                        return false;
                    }
                    tabu_.list().grow_extra();
                    ++number_of_non_improving_episodes_;
                } else {
                    if (run.cost < search.upper_bound() &&
                        run.cost + params_->tolerance < best_cost_) {
                        best_cost_ = run.cost;
                        number_of_non_improving_episodes_ = 0;
                        tabu_.list().shrink_extra();
                    } else {
                        ++number_of_non_improving_episodes_;
                    }
                    tabu_detail::add(tabu_, search, run.arc_ids);
                }
                if (number_of_non_improving_episodes_ >=
                    params_->non_improving_episodes_before_growth) {
                    tabu_.list().grow_extra();
                    number_of_non_improving_episodes_ = 0;
                }
                tabu_.age();
            }
            // Reseeded even after the last episode: the start labels then wait, so a solve that
            // spends max_iterations ends Truncated, not Complete.
            search.reseed();
            return number_of_runs - 1 < params_->max_iterations;
        }

    private:
        const AlgorithmBaseParams* params_;
        detail::TabuArcs tabu_;
        /// The cost of the best path found so far in this solve.
        double best_cost_ = std::numeric_limits<double>::infinity();
        /// How many episodes in a row have not improved on @ref best_cost_.
        size_t number_of_non_improving_episodes_ = 0;
};

}  // namespace rcspp::parts
