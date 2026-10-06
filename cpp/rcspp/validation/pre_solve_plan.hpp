// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "rcspp/graph/graph.hpp"
#include "rcspp/preprocessor/pre_solve_stage.hpp"

namespace rcspp {

/// @brief The stages one solve runs on the model before the search, in the order added.
///
/// A solve inspects every stage first (@ref inspect), which only reads the graph, then runs
/// every stage (@ref run), which may change it, and undoes the stages that ran (@ref undo), last
/// first, however the solve ends: the destructor undoes whatever is left, so an exception never
/// leaves arcs removed from the caller's graph.
///
/// @tparam ResourceType The resource type used in the graph.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class PreSolvePlan {
    public:
        PreSolvePlan() = default;
        PreSolvePlan(const PreSolvePlan&) = delete;
        PreSolvePlan& operator=(const PreSolvePlan&) = delete;
        PreSolvePlan(PreSolvePlan&&) = delete;
        PreSolvePlan& operator=(PreSolvePlan&&) = delete;

        /// @brief Undoes the stages that ran and are not undone yet (see @ref undo).
        ~PreSolvePlan() { undo(); }

        /// @brief Adds @p stage at the end. Stages are added before the plan runs.
        ///
        /// @param stage The stage.
        void add(std::unique_ptr<PreSolveStage<ResourceType>> stage) {
            stages_.push_back(std::move(stage));
        }

        /// @brief Runs every stage's read-only part on @p graph, in order.
        ///
        /// @param graph   The model: its live arcs and the arcs preprocessing removed.
        /// @param context What the stages of this solve share.
        void inspect(const Graph<ResourceType>& graph, SolveContext& context) const {
            for (const auto& stage : stages_) {
                stage->inspect(graph, context);
            }
        }

        /// @brief Runs every stage on @p graph, in order.
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
        std::vector<std::unique_ptr<PreSolveStage<ResourceType>>> stages_;
        size_t ran_ = 0;  ///< How many stages, from the first, have run and are not undone.
};

}  // namespace rcspp
