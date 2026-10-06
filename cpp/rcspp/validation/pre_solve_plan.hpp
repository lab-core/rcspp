// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "rcspp/algorithm/search_direction.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/preprocessor/pre_solve_stage.hpp"
#include "rcspp/validation/backward_extension_check.hpp"
#include "rcspp/validation/join_check.hpp"
#include "rcspp/validation/model_report.hpp"

namespace rcspp {

/// @brief The stages one solve runs on the model before the search, in order.
///
/// Built from the search's direction, a plan already holds the checks that direction needs: none
/// for a forward search; @c BackwardExtensionCheck for a backward one, which extends labels
/// backward; and @c JoinCheck as well for a bidirectional one, which also joins the two halves.
/// More stages can be added, never removed.
///
/// A solve uses a plan in three steps:
///  1. @ref inspect or @ref enforce: every stage reads the graph, without changing it;
///  2. @ref run: every stage may change the graph;
///  3. @ref undo: the stages that ran are undone, the last one first.
///
/// The destructor undoes whatever is left, so an exception never leaves arcs removed from the
/// caller's graph.
///
/// @tparam ResourceType The resource type used in the graph.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class PreSolvePlan {
    public:
        /// @brief A plan with no check: what a forward search needs.
        PreSolvePlan() = default;

        /// @brief A plan holding the checks a search in @p direction needs.
        ///
        /// @param direction The search's direction.
        explicit PreSolvePlan(SearchDirection direction) : direction_(direction) {
            if (direction != SearchDirection::Forward) {
                // Only a backward search returns the paths its backward labels end at a source; a
                // bidirectional one also finds them through its forward half and the join.
                add(std::make_unique<BackwardExtensionCheck<ResourceType>>(
                    /*backward_labels_end_paths=*/direction == SearchDirection::Backward));
            }
            if (direction == SearchDirection::Bidirectional) {
                add(std::make_unique<JoinCheck<ResourceType>>());
            }
        }

        PreSolvePlan(const PreSolvePlan&) = delete;
        PreSolvePlan& operator=(const PreSolvePlan&) = delete;
        PreSolvePlan(PreSolvePlan&&) = delete;
        PreSolvePlan& operator=(PreSolvePlan&&) = delete;

        /// @brief Undoes the stages that ran and are not undone yet (see @ref undo).
        ~PreSolvePlan() { undo(); }

        /// @brief Adds @p stage at the end of the plan.
        ///
        /// @param stage The stage.
        void add(std::unique_ptr<PreSolveStage<ResourceType>> stage) {
            stages_.push_back(std::move(stage));
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
        /// @throws ModelRefused listing every problem, for the plan's direction.
        void enforce(const Graph<ResourceType>& graph, SolveContext& context) const {
            inspect(graph, context);
            if (!context.report.ok()) {
                throw ModelRefused(direction_, context.report.problems);
            }
        }

        /// @brief Runs every stage on @p graph, in order; a check changes nothing.
        ///
        /// A stage counts as run before it starts, so one that throws part-way is undone too.
        ///
        /// @param graph   The graph about to be searched.
        /// @param context What the stages of this solve share.
        void run(Graph<ResourceType>& graph, SolveContext& context) {
            for (const auto& stage : stages_) {
                ++ran_;
                stage->run(graph, context);
            }
        }

        /// @brief Undoes every stage that ran, the last one first; a second call does nothing.
        void undo() noexcept {
            for (; ran_ > 0; --ran_) {
                stages_[ran_ - 1]->undo();
            }
        }

    private:
        SearchDirection direction_ = SearchDirection::Forward;
        std::vector<std::unique_ptr<PreSolveStage<ResourceType>>> stages_;
        size_t ran_ = 0;  ///< How many stages, from the first, have run and are not undone.
};

}  // namespace rcspp
