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

/// @brief Improving Tabu's driver: a first dive with no arc tabu (the construction), then
///        @c max_iterations tabu episodes, as @ref TabuEpisodes, that count how long they go
///        without improving.
///
/// The construction's path is the best cost so far; its arcs become tabu when it is below the
/// upper bound. After each later episode, a path strictly cheaper than the best (by
/// @c tolerance) is an improvement: it becomes the best and shrinks the adaptive tenure; any
/// other episode counts as one more without improving, and @c non_improving_episodes_before_growth
/// of them in a row grow the tenure. An episode's dive stops at its first path, whatever it costs:
/// only the count reads the best cost. @c max_iterations must be finite.
///
/// The construction does not age the list, so its arcs stay tabu for the full tenure; a later
/// episode's arcs, as in @ref TabuEpisodes, for one less.
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
                    return false;  // no feasible path exists, or (stored aside) the labels wait
                }
                best_cost_ = run.cost;
                if (best_cost_ < search.upper_bound()) {
                    tabu_detail::add(tabu_, search, run.arc_ids);
                }
            } else {
                if (!run.recorded) {
                    if (tabu_.list().empty()) {
                        // Out of paths with no arc tabu: no path is left, or (stored aside) the
                        // next episode would repeat this one, and the labels stored aside wait.
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
        double best_cost_ = std::numeric_limits<double>::infinity();  ///< the best cost found so
                                                                      ///< far in this solve
        /// episodes in a row that did not improve @ref best_cost_
        size_t number_of_non_improving_episodes_ = 0;
};

}  // namespace rcspp::parts
