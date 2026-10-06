// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "rcspp/algorithm/search_direction.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/validation/backward_extension_check.hpp"
#include "rcspp/validation/join_check.hpp"
#include "rcspp/validation/model_check.hpp"
#include "rcspp/validation/model_report.hpp"

namespace rcspp {

/// @brief The model checks a search in @p direction needs, in the order they run.
///
/// A forward search needs none. A backward search extends labels backward, so it needs
/// @c BackwardExtensionCheck. A bidirectional search also joins the two halves, so it needs
/// @c JoinCheck as well.
///
/// @tparam ResourceType The resource type used in the graph.
/// @param direction The search's direction.
/// @return The checks.
template <typename ResourceType>
[[nodiscard]] std::vector<std::unique_ptr<ModelCheck<ResourceType>>> checks_for(
    SearchDirection direction) {
    std::vector<std::unique_ptr<ModelCheck<ResourceType>>> checks;
    if (direction == SearchDirection::Backward || direction == SearchDirection::Bidirectional) {
        checks.push_back(std::make_unique<BackwardExtensionCheck<ResourceType>>());
    }
    if (direction == SearchDirection::Bidirectional) {
        checks.push_back(std::make_unique<JoinCheck<ResourceType>>());
    }
    return checks;
}

/// @brief Runs the checks a search in @p direction needs on @p graph, into one report.
///
/// @param graph     The model: its live arcs and the arcs preprocessing removed.
/// @param direction The search's direction.
/// @return Every problem found; empty when the model passes.
template <typename ResourceType>
[[nodiscard]] ModelReport run_checks(const Graph<ResourceType>& graph, SearchDirection direction) {
    ModelReport report;
    for (const auto& check : checks_for<ResourceType>(direction)) {
        check->check(graph, &report);
    }
    return report;
}

/// @brief Runs the checks a search in @p direction needs, and refuses a model that fails any.
///
/// @param graph     The model.
/// @param direction The search's direction.
/// @throws ModelRefused listing every problem, under @c refusal_header(direction).
template <typename ResourceType>
void enforce_checks(const Graph<ResourceType>& graph, SearchDirection direction) {
    ModelReport report = run_checks(graph, direction);
    if (!report.ok()) {
        throw ModelRefused(refusal_header(direction), std::move(report.problems));
    }
}

}  // namespace rcspp
