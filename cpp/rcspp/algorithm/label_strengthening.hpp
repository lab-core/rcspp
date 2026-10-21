// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>

#include "rcspp/label/label.hpp"

namespace rcspp {

/// @brief A change applied to every forward label that passes its feasibility test, before
///        dominance compares it with the labels already stored at its node.
///
/// A strengthening may only make a label forbid moves it could never complete, so that the set
/// of feasible paths is unchanged while dominance discards more labels. `ResourceGraph::solve`
/// installs one on the algorithm for the duration of a solve (see
/// `ResourceGraph::enable_ng_unreachable_fold`); a direct `Algorithm::solve` call runs without.
///
/// @tparam ResourceType The resource type carried by labels.
template <typename ResourceType>
class LabelStrengthening {
    public:
        virtual ~LabelStrengthening() = default;

        /// @brief Strengthens @p label, a feasible forward label not yet compared for dominance.
        ///
        /// @param label The label.
        /// @return How many changes were made, for `SolveResult::unreachable_folds`.
        virtual size_t strengthen_forward(Label<ResourceType>* label) = 0;
};

}  // namespace rcspp
