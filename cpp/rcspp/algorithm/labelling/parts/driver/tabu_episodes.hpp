// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <vector>

#include "rcspp/algorithm/algorithm_params.hpp"
#include "rcspp/algorithm/arc_mask.hpp"
#include "rcspp/algorithm/labelling/parts/driver/driver.hpp"
#include "rcspp/algorithm/limits.hpp"
#include "rcspp/algorithm/proof_record.hpp"
#include "rcspp/algorithm/tabu/tabu_arcs.hpp"

namespace rcspp::parts {

namespace tabu_detail {

/// @brief Starts @p tabu for a solve of @p search, whose runs then read its mask, soft where an
///        arc is tabu.
template <typename SearchType>
void begin(detail::TabuArcs& tabu, SearchType& search) {
    tabu.begin(search.graph(), search.get_arc_mask());
    search.use_arc_mask(&tabu.mask());
}

/// @brief Makes the arcs of the path @p search found tabu; the first arc made tabu loses its
///        proof.
template <typename SearchType>
void add(detail::TabuArcs& tabu, SearchType& search, const std::vector<size_t>& arc_ids) {
    if (tabu.add(search.graph(), arc_ids)) {
        search.lose_proof(ProofLoss::HeuristicDriver);
    }
}

}  // namespace tabu_detail

/// @brief Tabu's driver: episodes of a dive, each ending at its first path, whose arcs are then
///        tabu (soft in the search's mask) for @c tabu_tenure episodes.
///
/// After an episode that found a path, its arcs become tabu, and the adaptive tenure shrinks when
/// the path was new and grows when it was not; after one that ran out of paths, the solve ends if
/// no arc is tabu (no path exists), and the tenure grows otherwise. An episode that found no path
/// while a cut held labels back (@c RunEnd::ended_with_stored_aside: the per-node quota, past @c
/// max_phases) counts as one that ran out of paths, except that with no arc tabu its labels still
/// wait, so the status says the solve was truncated. Then the tenures age, and the next episode
/// starts from the sources. @c max_iterations counts episodes, and must be finite. A run stopped by
/// a budget, an interrupt or the memory limit ends the solve, its labels still waiting, so the
/// status says why.
///
/// The search's schedule drops a child reached along a tabu arc unless every child is
/// (@ref TabuChildFilter).
class TabuEpisodes {
    public:
        explicit TabuEpisodes(const AlgorithmBaseParams& params)
            : params_(&params), tabu_(params, ArcMask::State::Soft) {}

        template <typename SearchType>
        bool begin(SearchType& search) {
            tabu_detail::begin(tabu_, search);
            search.end_runs_at_end_node();
            if (params_->max_iterations >= kNoLimit) {
                LOG_ERROR("max_iterations must be set to a finite value for strategy::Tabu.\n");
                return false;
            }
            return params_->max_iterations > 0;
        }

        template <typename SearchType>
        bool after_run(SearchType& search, size_t number_of_runs) {
            const RunEnd& run = search.last_run();
            if (run.recorded) {
                tabu_detail::add(tabu_, search, run.arc_ids);
                if (run.new_solution) {
                    tabu_.list().shrink_extra();
                } else {
                    tabu_.list().grow_extra();
                }
            } else if (!run.exhausted && !run.ended_with_stored_aside) {
                return false;  // stopped: the labels wait, and the status says why
            } else if (tabu_.list().empty()) {
                // Out of paths with no arc tabu: no feasible path exists, or (held back) the next
                // episode would repeat this one, and the labels held back wait.
                return false;
            } else {
                tabu_.list().grow_extra();
            }
            tabu_.age();
            search.reseed();
            return number_of_runs < params_->max_iterations;
        }

    private:
        const AlgorithmBaseParams* params_;
        detail::TabuArcs tabu_;
};

}  // namespace rcspp::parts
