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

/// @brief Starts @p tabu for a new solve of @p search: no arc is tabu yet, and the search's runs
///        read the tabu arcs' mask, a copy of the caller's mask in which each tabu arc is soft.
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

/// @brief The driver of @c strategy::Tabu: it runs dive after dive, and after each one forbids
///        the arcs of the path found for the next few dives, so that each dive looks for a
///        different path.
///
/// Each run, called an @b episode, is a dive from the start labels that stops at the first path
/// it finds (@c Driven::end_runs_at_end_node). A forbidden, or @b tabu, arc is a soft arc of the
/// search's arc mask. The schedule's child filter (@ref TabuChildFilter) drops a child reached
/// along a tabu arc, unless every child was reached along one.
///
/// After each episode, what happens depends on how it ended (@c RunEnd):
/// - It found a path: the path's arcs become tabu, except those with an end in
///   @c tabu_exempt_node_ids. The adaptive extra tenure shrinks by one if the path was a new
///   solution, and grows by one if it was not (the solve already had it, or it does not cost
///   less than the upper bound).
/// - It found no path, and no label was left: if no arc is tabu, the solve ends, since the next
///   episode would be the same. Otherwise the extra tenure grows by one.
/// - It found no path, and labels stored aside (by the per-node quota or memory pressure) were
///   left after its last phase: the same as above, except that when no arc is tabu, those
///   labels are left waiting, so the solve ends @c Truncated.
/// - A limit stopped it (the time limit, an interrupt, the memory limit): the solve ends, and
///   its status names the limit.
/// If the solve goes on, every tenure is shortened by one episode, and the next episode starts
/// again from the start labels (@c Driven::reseed).
///
/// An arc stays tabu for its tenure minus one episode. The tenure is @c tabu_tenure plus the
/// adaptive extra (and, with @c tabu_random_noise, one more or one less at random). The episode
/// that makes an arc tabu already shortens its tenure once, so with a tenure of 1 the arc is
/// never tabu.
///
/// @c max_iterations is the number of episodes, and must be finite. A solve that makes them all
/// ends @c Truncated, since the next episode's start labels are left waiting.
class TabuEpisodes {
    public:
        /// @brief A driver that reads @p params: the iteration budget, the tenure, its noise and
        ///        seed, and the exempt nodes.
        ///
        /// @param params The algorithm's params; must outlive the driver.
        explicit TabuEpisodes(const AlgorithmBaseParams& params)
            : params_(&params), tabu_(params, ArcMask::State::Soft) {}

        /// @brief Starts a solve: no arc tabu yet, the search reading the tabu arcs' mask, and
        ///        each run ending at its first path.
        ///
        /// @param search The driven search.
        /// @return Whether to run: false when @c max_iterations is 0, or not finite (logged).
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

        /// @brief Ends an episode: makes its path's arcs tabu and adapts the tenure, or decides
        ///        the solve is over; then ages the tenures and reseeds the next episode.
        ///
        /// @param search         The driven search.
        /// @param number_of_runs The episodes run so far, this one included.
        /// @return Whether to run another episode.
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
                // No path found, and no arc is tabu: the next episode would repeat this one, so
                // the solve ends. Labels stored aside, if any, are left waiting (Truncated).
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
