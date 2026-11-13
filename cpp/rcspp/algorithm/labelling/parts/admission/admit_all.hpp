// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/labelling/parts/admission/admission.hpp"
#include "rcspp/algorithm/labelling/parts/admission/node_view.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp::parts {

/// @brief Keeps every feasible label, comparing none: a heuristic's admission. It is exact, since
/// it
///        keeps every label exact dominance keeps (and more).
struct AdmitAll {
        /// @brief Whether the rule keeps every label exact dominance keeps: always.
        [[nodiscard]] static bool is_exact(const AdmissionContext& /*context*/) { return true; }

        /// @brief Keeps @p candidate.
        template <typename ResourceType, typename NodeViewType>
        bool admit(const Label<ResourceType>& /*candidate*/, NodeViewType& /*node*/,
                   const AdmissionContext& /*context*/) const {
            return true;
        }
};

}  // namespace rcspp::parts
