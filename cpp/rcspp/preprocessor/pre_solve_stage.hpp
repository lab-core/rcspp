// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>

#include "rcspp/graph/graph.hpp"
#include "rcspp/validation/model_report.hpp"

namespace rcspp {

/// @brief What the pre-solve stages of one solve share: the cost bound and the checks' report.
struct SolveContext {
        /// @brief The solve's cost upper bound, for a stage that needs it (no built-in stage
        ///        reads it).
        double upper_bound = std::numeric_limits<double>::infinity();
        /// @brief What the checks found.
        ModelReport report;
};

/// @brief One step run on the graph before a search: a check of the model, or a reduction of
///        the graph.
///
/// A stage has two parts, both empty by default. @ref inspect only reads the graph: a check
/// reports its problems there, and a caller holding a const graph can run it too. @ref run may
/// change the graph: a reduction removes arcs there, and @ref undo puts them back.
///
/// @tparam ResourceType The resource type used in the graph.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class PreSolveStage {
    public:
        virtual ~PreSolveStage() = default;

        /// @brief Reads @p graph without changing it; nothing by default.
        ///
        /// @param graph   The model: its live arcs and the arcs preprocessing removed.
        /// @param context What the stages of this solve share.
        virtual void inspect(const Graph<ResourceType>& /*graph*/,
                             SolveContext& /*context*/) const {}

        /// @brief Changes @p graph before the search; nothing by default.
        ///
        /// @param graph   The graph about to be searched.
        /// @param context What the stages of this solve share.
        virtual void run(Graph<ResourceType>& /*graph*/, SolveContext& /*context*/) {}

        /// @brief Undoes what @ref run did to the graph; nothing by default.
        ///
        /// Must not throw: it runs from a destructor when the search throws.
        virtual void undo() noexcept {}
};

}  // namespace rcspp
