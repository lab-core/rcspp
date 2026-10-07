// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <string>

#include "rcspp/algorithm/search_direction.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/preprocessor/pre_solve_stage.hpp"
#include "rcspp/validation/model_report.hpp"

namespace rcspp {

/// @brief A check of the model a search needs before it starts: the Check kind of pre-solve
///        stage.
///
/// A check never changes the graph. It reads the whole model, the live arcs and the arcs
/// preprocessing removed alike, so its verdict does not depend on what one solve's bound happened
/// to remove.
///
/// @tparam ResourceType The resource type used in the graph.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class ModelCheck : public PreSolveStage<ResourceType> {
    public:
        /// @brief A check reads the model.
        [[nodiscard]] StageKind kind() const final { return StageKind::Check; }

        /// @brief Runs @ref check, adding its problems to the context's report.
        void run(Graph<ResourceType>& graph, SolveContext& context) final {
            check(graph, &context.report);
        }

        /// @brief Adds one line to @p report for each problem found in the model.
        ///
        /// @param graph  The model.
        /// @param report Receives the problems; earlier checks' lines stay in it.
        virtual void check(const Graph<ResourceType>& graph, ModelReport* report) const = 0;
};

/// @brief The first line of the message when a search in @p direction refuses a model.
///
/// @param direction The search's direction.
/// @return E.g. "a bidirectional search cannot run on this model:".
[[nodiscard]] inline std::string refusal_header(SearchDirection direction) {
    return "a " + std::string(to_string(direction)) + " search cannot run on this model:";
}

}  // namespace rcspp
