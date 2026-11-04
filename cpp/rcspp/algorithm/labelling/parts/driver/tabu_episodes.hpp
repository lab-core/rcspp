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
#include "rcspp/algorithm/tabu/tabu_list.hpp"

namespace rcspp::detail {

/// @brief The tabu arcs of a driven search: a tabu list (@ref TabuList) and the soft mask the
///        search reads, kept in step.
///
/// The mask starts as the one the algorithm was given (@c Algorithm::set_arc_mask), or open; an
/// arc made tabu goes soft, and goes back to that state when its tenure runs out. An arc of a path
/// found is never hard, so a tabu arc never hides a hard one.
class TabuArcs {
    public:
        explicit TabuArcs(const AlgorithmBaseParams& params)
            : params_(&params), tabu_(params.seed) {}

        /// @brief Empties the list, and has @p search's runs read the mask, which starts as the
        ///        one it was given.
        template <typename SearchType>
        void begin(SearchType& search) {
            tabu_.clear();
            given_ = search.get_arc_mask();
            const size_t number_of_arc_ids = search.graph().get_arc_id_bound();
            mask_.reset(number_of_arc_ids);
            if (given_ != nullptr) {
                for (size_t arc_id = 0; arc_id < number_of_arc_ids; ++arc_id) {
                    mask_.set(arc_id, given_->state(arc_id));
                }
            }
            search.use_arc_mask(&mask_);
        }

        /// @brief Makes the arcs of a path tabu, all but those with an exempt end
        ///        (@c tabu_exempt_node_ids); the first arc made tabu loses @p search's proof.
        template <typename SearchType>
        void add(SearchType& search, const std::vector<size_t>& arc_ids) {
            const auto& exempt = params_->tabu_exempt_node_ids;
            for (const size_t arc_id : arc_ids) {
                const auto* arc = search.graph().get_arc(arc_id);
                if (exempt.contains(arc->origin->id) || exempt.contains(arc->destination->id)) {
                    continue;
                }
                tabu_.add(arc_id, params_->tabu_tenure, params_->tabu_random_noise);
                if (tabu_.is_tabu(arc_id)) {
                    mask_.set(arc_id, ArcMask::State::Soft);
                    search.lose_proof(ProofLoss::HeuristicDriver);
                }
            }
        }

        /// @brief One episode older: the tenures shorten, and each arc whose tenure ran out goes
        ///        back to its state in the mask given.
        void age() {
            tabu_.age([this](size_t arc_id) {
                mask_.set(arc_id, given_ != nullptr ? given_->state(arc_id) : ArcMask::State::Open);
            });
        }

        [[nodiscard]] TabuList& list() { return tabu_; }

    private:
        const AlgorithmBaseParams* params_;
        TabuList tabu_;
        ArcMask mask_;
        const ArcMask* given_ = nullptr;
};

}  // namespace rcspp::detail

namespace rcspp::parts {

/// @brief Tabu's driver: episodes of a dive, each ending at its first path, whose arcs are then
///        tabu (soft in the search's mask) for @c tabu_tenure episodes.
///
/// After an episode that found a path, its arcs become tabu, and the adaptive tenure shrinks when
/// the path was new and grows when it was not; after one that ran out of paths, the solve ends if
/// no arc is tabu (no path exists), and the tenure grows otherwise. Then the tenures age, and the
/// next episode starts from the sources. @c max_iterations counts episodes, and must be finite.
/// A run stopped by a budget, an interrupt or the memory limit ends the solve, its labels still
/// waiting, so the status says why.
///
/// The search's schedule drops a child reached along a tabu arc unless every child is
/// (@ref TabuChildFilter).
class TabuEpisodes {
    public:
        explicit TabuEpisodes(const AlgorithmBaseParams& params)
            : params_(&params), tabu_(params) {}

        template <typename SearchType>
        bool begin(SearchType& search) {
            tabu_.begin(search);
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
                tabu_.add(search, run.arc_ids);
                if (run.new_solution) {
                    tabu_.list().shrink_extra();
                } else {
                    tabu_.list().grow_extra();
                }
            } else if (!run.exhausted) {
                return false;  // stopped: the labels wait, and the status says why
            } else if (tabu_.list().empty()) {
                return false;  // out of paths with no arc tabu: no feasible path exists
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
