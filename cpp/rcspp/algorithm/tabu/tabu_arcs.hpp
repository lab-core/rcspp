// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <vector>

#include "rcspp/algorithm/algorithm_params.hpp"
#include "rcspp/algorithm/arc_mask.hpp"
#include "rcspp/algorithm/tabu/tabu_list.hpp"
#include "rcspp/graph/graph.hpp"

namespace rcspp::detail {

/// @brief The tabu arcs of a search, and the arc mask that makes the search avoid them.
///
/// It holds a tabu list (@ref TabuList), which counts how long each arc stays tabu, and an arc
/// mask that the search reads. The mask starts as a copy of the mask the algorithm was given
/// (@c Algorithm::set_arc_mask), or all open. When an arc is made tabu, its state in the mask
/// becomes the one the owner chose:
/// - soft for the tabu searches' drivers, whose dive still uses the arc when every child is
///   reached along a tabu arc;
/// - hard for @ref DiversificationSearch, which leaves the arc out.
///
/// When its tenure runs out, the arc goes back to its state in the given mask. A path found
/// never uses an arc that the given mask makes hard, so making an arc tabu never turns a hard
/// arc into a soft one.
class TabuArcs {
    public:
        /// @brief Tabu arcs with the params' tenure, noise, seed and exempt nodes.
        ///
        /// @param params The algorithm's params; must outlive this object.
        /// @param tabu_state The state a tabu arc takes in the mask.
        TabuArcs(const AlgorithmBaseParams& params, ArcMask::State tabu_state)
            : params_(&params), tabu_state_(tabu_state), tabu_(params.seed) {}

        /// @brief Empties the list, its adaptive extra tenure included, and starts the mask as
        ///        @p given (open when null). The noise's random generator is not reseeded: it
        ///        goes on from the previous solve.
        ///
        /// @param graph The graph being solved, which sizes the mask.
        /// @param given The mask the algorithm was given, or null; must outlive the solve.
        template <typename ResourceType>
        void begin(const Graph<ResourceType>& graph, const ArcMask* given) {
            tabu_.clear();
            given_ = given;
            const size_t number_of_arc_ids = graph.get_arc_id_bound();
            mask_.reset(number_of_arc_ids);
            if (given_ != nullptr) {
                for (size_t arc_id = 0; arc_id < number_of_arc_ids; ++arc_id) {
                    mask_.set(arc_id, given_->state(arc_id));
                }
            }
        }

        /// @brief Makes the arcs of a path tabu, all but those with an exempt end
        ///        (@c tabu_exempt_node_ids) and any id @p graph has no arc for.
        ///
        /// @param graph The graph being solved.
        /// @param arc_ids The path's arc ids.
        /// @return Whether an arc was made tabu: the owner then loses its proof.
        template <typename ResourceType>
        bool add(const Graph<ResourceType>& graph, const std::vector<size_t>& arc_ids) {
            const auto& exempt = params_->tabu_exempt_node_ids;
            bool made_tabu = false;
            for (const size_t arc_id : arc_ids) {
                const auto* arc = graph.get_arc(arc_id);
                if (arc == nullptr || exempt.contains(arc->origin->id) ||
                    exempt.contains(arc->destination->id)) {
                    continue;
                }
                tabu_.add(arc_id, params_->tabu_tenure, params_->tabu_random_noise);
                if (tabu_.is_tabu(arc_id)) {
                    mask_.set(arc_id, tabu_state_);
                    made_tabu = true;
                }
            }
            return made_tabu;
        }

        /// @brief Ages the tabu list by one step: every tenure shortens by one, and each arc whose
        ///        tenure ran out goes back to its state in the given mask.
        ///
        /// A step is an episode for the tabu searches' drivers, and an inner solve for
        /// @ref DiversificationSearch.
        void age() {
            tabu_.age([this](size_t arc_id) {
                mask_.set(arc_id, given_ != nullptr ? given_->state(arc_id) : ArcMask::State::Open);
            });
        }

        /// @brief The mask the search reads.
        [[nodiscard]] const ArcMask& mask() const { return mask_; }

        /// @brief The tabu list, for its adaptive tenure.
        [[nodiscard]] TabuList& list() { return tabu_; }

    private:
        const AlgorithmBaseParams* params_;
        ArcMask::State tabu_state_;
        TabuList tabu_;
        ArcMask mask_;
        const ArcMask* given_ = nullptr;
};

}  // namespace rcspp::detail
