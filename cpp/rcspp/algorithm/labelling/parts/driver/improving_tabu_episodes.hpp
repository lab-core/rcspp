// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <limits>

#include "rcspp/algorithm/algorithm_params.hpp"
#include "rcspp/algorithm/labelling/parts/driver/driver.hpp"
#include "rcspp/algorithm/labelling/parts/driver/tabu_episodes.hpp"
#include "rcspp/algorithm/limits.hpp"

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
class ImprovingTabuEpisodes {
    public:
        explicit ImprovingTabuEpisodes(const AlgorithmBaseParams& params)
            : params_(&params), tabu_(params) {}

        template <typename SearchType>
        bool begin(SearchType& search) {
            tabu_.begin(search);
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

        template <typename SearchType>
        bool after_run(SearchType& search, size_t number_of_runs) {
            const RunEnd& run = search.last_run();
            if (!run.recorded && !run.exhausted) {
                return false;  // stopped: the labels wait, and the status says why
            }
            if (number_of_runs == 1) {  // the construction
                if (!run.recorded) {
                    return false;  // no feasible path exists
                }
                best_cost_ = run.cost;
                if (best_cost_ < search.upper_bound()) {
                    tabu_.add(search, run.arc_ids);
                }
            } else {
                if (!run.recorded) {
                    if (tabu_.list().empty()) {
                        return false;  // out of paths with no arc tabu: nothing is left to find
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
                    tabu_.add(search, run.arc_ids);
                }
                if (number_of_non_improving_episodes_ >=
                    params_->non_improving_episodes_before_growth) {
                    tabu_.list().grow_extra();
                    number_of_non_improving_episodes_ = 0;
                }
                tabu_.age();
            }
            search.reseed();
            return number_of_runs - 1 < params_->max_iterations;
        }

    private:
        const AlgorithmBaseParams* params_;
        detail::TabuArcs tabu_;
        double best_cost_ = std::numeric_limits<double>::infinity();
        size_t number_of_non_improving_episodes_ = 0;
};

}  // namespace rcspp::parts
