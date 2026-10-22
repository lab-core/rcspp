// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <vector>

#include "rcspp/graph/graph.hpp"
#include "rcspp/preprocessor/pre_solve_stage.hpp"
#include "rcspp/resource/functions/directional_resource_kind.hpp"
#include "rcspp/validation/model_report.hpp"
#include "rcspp/validation/threshold_probe.hpp"

namespace rcspp {

/// @brief A check of the model a search needs before it starts: a pre-solve stage that only
///        inspects.
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
        /// @brief Runs @ref check, adding its problems to the context's report.
        ///
        /// A check only reads, so it runs in the read-only part of a stage, on any graph,
        /// including one a caller holds as const.
        void inspect(const Graph<ResourceType>& graph, SolveContext& context) const final {
            check(graph, &context.report);
        }

        /// @brief Adds one line to @p report for each problem found in the model.
        ///
        /// @param graph  The model.
        /// @param report Receives the problems; earlier checks' lines stay in it.
        virtual void check(const Graph<ResourceType>& graph, ModelReport* report) const = 0;

    protected:
        /// @brief Where a check reads the model's declarations (each component's
        ///        @c directional_resource_kind(), @c join_rule() and the like). Every arc carries
        ///        the same extender composition and every node the same resource composition, so
        ///        one arc and one node stand for all.
        struct Declarations {
                /// @brief An arc of the model, live or removed; @c nullptr when it has none.
                const Arc<ResourceType>* arc = nullptr;
                /// @brief A node carrying a resource; @c nullptr when none does.
                const Node<ResourceType>* node = nullptr;
                /// @brief Each component's @c directional_resource_kind(), read from @ref arc.
                std::vector<DirectionalResourceKind> kinds;

                /// @brief Component @p index's directional resource kind; @c Unspecified past the
                ///        arc's components.
                [[nodiscard]] DirectionalResourceKind kind(size_t index) const {
                    return index < kinds.size() ? kinds[index]
                                                : DirectionalResourceKind::Unspecified;
                }

                /// @brief Calls @p visit with each component of @ref node's resource, its
                ///        directional resource kind and its index.
                template <typename Visit>
                void for_each_component(Visit&& visit) const {
                    size_t index = 0;
                    node->resource->for_each_component([&](const auto& component) {
                        visit(component, kind(index), index);
                        ++index;
                    });
                }
        };

        /// @brief Reads where @p graph's declarations are.
        ///
        /// @param graph The model.
        /// @return One arc, one node with a resource, and the arc's directional resource kinds.
        [[nodiscard]] static Declarations declarations(const Graph<ResourceType>& graph) {
            return {.arc = threshold_probe::model_arc(graph),
                    .node = threshold_probe::node_with_resource(graph),
                    .kinds = threshold_probe::model_kinds(graph)};
        }
};

}  // namespace rcspp
