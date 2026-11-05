// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/label_buckets.hpp"
#include "rcspp/algorithm/labelling/labelling_algorithm.hpp"
#include "rcspp/algorithm/labelling/schedules.hpp"
#include "rcspp/algorithm/labelling/search.hpp"

namespace rcspp {

/// @brief Label-correcting dominance algorithm with A*-style priority ordering.
///
/// One search on a best-first schedule: the labels are taken lowest @f$ f = g + h @f$ first,
/// where @f$ g @f$ is the label's cost and @f$ h(n) @f$ a lower bound on the cost still to pay
/// from @p n to a sink, computed once per solve (@ref detail::completion_bounds).
///
/// Expanding the smallest-@f$ f @f$ labels first typically extends far fewer labels than FIFO
/// ordering when arc costs are heterogeneous. Two things are worth being precise about:
///
///  - This is a *label-correcting* search: the whole frontier is processed (it does not stop at
///    the first sink reached), so a FULL search returns the optimal solution for ANY @f$ h @f$ —
///    the heuristic only changes the *order* of expansion, not which labels exist.
///  - @f$ h @f$ matters for correctness only together with label-dropping truncation (a per-node
///    extension cap, memory-pressure pruning, or @ref AlgorithmBaseParams::max_solutions):
///    those keep the lowest-@f$ f @f$ labels, so an *admissible* @f$ h @f$ (a true lower bound)
///    keeps the most promising labels and the truncated result stays optimal far more often,
///    whereas an over-estimating @f$ h @f$ could drop the optimal path.
///
/// The bound is computed on the objective, the cost the labels carry, so it is a lower bound
/// whichever components the cost function reads. If the relaxation contains a negative-cost
/// cycle there is no finite bound, and the search orders by cost alone, as the non-A* dominance
/// algorithms do.
///
/// @tparam ResourceType       Composed resource type (must satisfy ResourceTypeConcept).
/// @tparam LabelContainerType Non-dominated label container (default: LabelList).
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>>
    requires ResourceTypeConcept<ResourceType>
class AStarDominanceAlgorithm
    : public detail::LabellingAlgorithm<ResourceType, LabelContainerType, ForwardDirection,
                                        detail::Preset<parts::BestFirstSchedule<ResourceType>>> {
        using Base =
            detail::LabellingAlgorithm<ResourceType, LabelContainerType, ForwardDirection,
                                       detail::Preset<parts::BestFirstSchedule<ResourceType>>>;

    public:
        using Base::Base;
};

}  // namespace rcspp
