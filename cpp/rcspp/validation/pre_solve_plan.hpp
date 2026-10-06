// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "rcspp/algorithm/search_direction.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/preprocessor/pre_solve_stage.hpp"
#include "rcspp/validation/backward_extension_check.hpp"
#include "rcspp/validation/join_check.hpp"
#include "rcspp/validation/model_check.hpp"
#include "rcspp/validation/model_report.hpp"

namespace rcspp {

/// @brief What one solve runs on the model before the search.
///
/// Built from the search's direction, a plan already holds the checks that direction needs: none
/// for a forward search; @c BackwardExtensionCheck for a backward one, which extends labels
/// backward; and @c JoinCheck as well for a bidirectional one, which also joins the two halves.
/// Stages can be added, never removed, and every Check stage runs before any Reduce stage.
///
/// @tparam ResourceType The resource type used in the graph.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class PreSolvePlan {
    public:
        /// @brief A plan holding the checks a search in @p direction needs.
        ///
        /// @param direction The search's direction.
        explicit PreSolvePlan(SearchDirection direction) : direction_(direction) {
            if (direction != SearchDirection::Forward) {
                add(std::make_unique<BackwardExtensionCheck<ResourceType>>());
            }
            if (direction == SearchDirection::Bidirectional) {
                add(std::make_unique<JoinCheck<ResourceType>>());
            }
        }

        /// @brief Adds @p stage after the stages of its kind: a Check stage before every Reduce
        ///        stage.
        ///
        /// A stage whose name is already in the plan is dropped, so no problem is reported twice.
        ///
        /// @param stage The stage.
        void add(std::unique_ptr<PreSolveStage<ResourceType>> stage) {
            const auto same_name = [&stage](const auto& other) {
                return other->name() == stage->name();
            };
            if (std::ranges::any_of(stages_, same_name)) {
                return;
            }
            if (stage->kind() == StageKind::Check) {
                const auto first_reduce = std::ranges::find_if(stages_, [](const auto& other) {
                    return other->kind() != StageKind::Check;
                });
                stages_.insert(first_reduce, std::move(stage));
            } else {
                stages_.push_back(std::move(stage));
            }
        }

        /// @brief Runs every stage's read-only part on @p graph, in order.
        ///
        /// The checks add their problems to the context's report. Nothing changes the graph, so a
        /// caller holding a const graph can inspect it too.
        ///
        /// @param graph   The model: its live arcs and the arcs preprocessing removed.
        /// @param context What the stages of this solve share.
        void inspect(const Graph<ResourceType>& graph, SolveContext& context) const {
            for (const auto& stage : stages_) {
                stage->inspect(graph, context);
            }
        }

        /// @brief Inspects @p graph, and refuses it if a check found a problem.
        ///
        /// @param graph   The model.
        /// @param context What the stages of this solve share; holds the problems afterwards.
        /// @throws ModelRefused listing every problem, under @c refusal_header(direction()).
        void enforce(const Graph<ResourceType>& graph, SolveContext& context) const {
            inspect(graph, context);
            if (!context.report.ok()) {
                throw ModelRefused(refusal_header(direction_), context.report.problems);
            }
        }

        /// @brief The direction of the search the plan was built for.
        [[nodiscard]] SearchDirection direction() const { return direction_; }

        /// @brief The stages, in the order they run.
        [[nodiscard]] const std::vector<std::unique_ptr<PreSolveStage<ResourceType>>>& stages()
            const {
            return stages_;
        }

    private:
        SearchDirection direction_;
        std::vector<std::unique_ptr<PreSolveStage<ResourceType>>> stages_;
};

}  // namespace rcspp
