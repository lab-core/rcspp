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

/// @brief Makes the arcs @p arc_ids (the path the last run found) tabu; once one is, the
///        solve's result is not a proof (@c ProofLoss::HeuristicDriver).
template <typename SearchType>
void add(detail::TabuArcs& tabu, SearchType& search, const std::vector<size_t>& arc_ids) {
    if (tabu.add(search.graph(), arc_ids)) {
        search.lose_proof(ProofLoss::HeuristicDriver);
    }
}

}  // namespace tabu_detail

/// @brief Tabu's driver: episodes of a dive, each ending at its first path, whose arcs are then
///        tabu (soft in the search's mask) for the next episodes: the tenure (@c tabu_tenure
///        plus the adaptive extra) minus one, since the episode that makes an arc tabu also ages
///        the list. With a tenure of 1 an arc is never tabu.
///
/// An episode is one run of the driven search (@c detail::Driven): a dive from the start labels
/// that ends at the first end-node label it records. The search's schedule releases a child
/// reached along a tabu arc, unless every child is (aspiration by default, @ref TabuChildFilter):
/// the child is not kept for a later episode.
///
/// After an episode that found a path, the path's arcs become tabu (all but those with an end in
/// @c tabu_exempt_node_ids), and the adaptive tenure shrinks when the path was a new solution and
/// grows when it was not (a duplicate, or not below the upper bound). After one that ran out of
/// paths, the solve ends if no arc is tabu (no path exists), and the tenure grows otherwise. An
/// episode that found no path while labels the per-node quota or memory pressure stored aside
/// were left after its last phase (@c RunEnd::ended_with_stored_aside) counts as one that ran out
/// of paths, except that with no arc tabu its labels still wait, so the status says the solve was
/// truncated. Then the tenures age (once per episode, the episode that made an arc tabu
/// included), and the next episode starts from the start labels again (@c reseed).
///
/// @c max_iterations counts episodes, and must be finite; a solve that spends it ends
/// @c Truncated, the next episode's start labels waiting. A run stopped by the time limit, an
/// interrupt or the memory limit ends the solve, its labels still waiting, so the solve's status
/// says which stop.
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
                // Out of paths with no arc tabu: no feasible path exists, or (stored aside) the
                // next episode would repeat this one, and the labels stored aside wait.
                return false;
            } else {
                tabu_.list().grow_extra();
            }
            tabu_.age();
            // Reseeded even after the last episode: the start labels then wait, so a solve that
            // spends max_iterations ends Truncated, not Complete.
            search.reseed();
            return number_of_runs < params_->max_iterations;
        }

    private:
        const AlgorithmBaseParams* params_;
        detail::TabuArcs tabu_;
};

}  // namespace rcspp::parts
