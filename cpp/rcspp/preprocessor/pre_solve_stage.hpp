// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <string_view>

#include "rcspp/graph/graph.hpp"
#include "rcspp/validation/model_report.hpp"

namespace rcspp {

/// @brief What a pre-solve stage does to the model.
enum class StageKind {
    Check,   ///< Reads the model and reports problems; never changes the graph.
    Reduce,  ///< Removes arcs that no useful path needs, and can put them back.
};

/// @brief What the stages run before one search share.
struct SolveContext {
        /// @brief The cost upper bound of the solve.
        double upper_bound = std::numeric_limits<double>::infinity();
        /// @brief What the Check stages found.
        ModelReport report;
};

/// @brief One step run on the graph before a search: a check of the model, or a reduction.
///
/// `ResourceGraph::solve` runs its Reduce stages only when `preprocess` is set, and undoes them
/// after the search, however the search ends.
///
/// @tparam ResourceType The resource type used in the graph.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class PreSolveStage {
    public:
        virtual ~PreSolveStage() = default;

        /// @brief Whether the stage checks the model or reduces the graph.
        [[nodiscard]] virtual StageKind kind() const = 0;

        /// @brief A short name, for messages.
        [[nodiscard]] virtual std::string_view name() const = 0;

        /// @brief Runs the stage on @p graph.
        ///
        /// @param graph   The graph about to be searched.
        /// @param context What the stages of this solve share; a Check stage adds its problems to
        ///                the report.
        virtual void run(Graph<ResourceType>& graph, SolveContext& context) = 0;

        /// @brief Undoes what @ref run did to the graph; nothing by default.
        ///
        /// Must not throw: it runs from a destructor when the search throws.
        virtual void undo() noexcept {}
};

}  // namespace rcspp
