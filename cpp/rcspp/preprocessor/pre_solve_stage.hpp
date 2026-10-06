// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

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
/// A stage has two parts. @ref inspect only reads the graph: every stage's runs first, and a caller
/// holding a const graph can run it too. @ref run may change the graph, and @ref undo puts it back.
/// `ResourceGraph::solve` runs the Reduce stages only when `preprocess` is set, and undoes them
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

        /// @brief Reads @p graph without changing it; nothing by default.
        ///
        /// A Check stage adds its problems to the context's report here.
        ///
        /// @param graph   The model: its live arcs and the arcs preprocessing removed.
        /// @param context What the stages of this solve share.
        virtual void inspect(const Graph<ResourceType>& /*graph*/,
                             SolveContext& /*context*/) const {}

        /// @brief Changes @p graph before the search; nothing by default.
        ///
        /// A Reduce stage removes arcs here.
        ///
        /// @param graph   The graph about to be searched.
        /// @param context What the stages of this solve share.
        virtual void run(Graph<ResourceType>& /*graph*/, SolveContext& /*context*/) {}

        /// @brief Undoes what @ref run did to the graph; nothing by default.
        ///
        /// Must not throw: it runs from a destructor when the search throws.
        virtual void undo() noexcept {}
};

/// @brief The stages one solve ran on the graph, undone however the solve ends.
///
/// @ref run runs a stage and keeps it. @ref undo_all undoes every kept stage, the last one first;
/// the destructor does the same for whatever is left, so an exception out of the search, or out
/// of a user's function inside a stage, never leaves arcs removed from the caller's graph.
///
/// @tparam ResourceType The resource type used in the graph.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class PreSolveScope {
    public:
        /// @brief A scope that runs stages on @p graph, sharing @p context.
        ///
        /// @param graph   The graph about to be searched; must outlive the scope.
        /// @param context What the stages of this solve share; must outlive the scope.
        PreSolveScope(Graph<ResourceType>& graph, SolveContext& context)
            : graph_(graph), context_(context) {}

        PreSolveScope(const PreSolveScope&) = delete;
        PreSolveScope& operator=(const PreSolveScope&) = delete;
        PreSolveScope(PreSolveScope&&) = delete;
        PreSolveScope& operator=(PreSolveScope&&) = delete;

        ~PreSolveScope() { undo_all(); }

        /// @brief Runs @p stage on the graph now, and keeps it to undo.
        ///
        /// The stage is kept before it runs, so one that throws part-way is undone too.
        ///
        /// @param stage The stage.
        void run(std::unique_ptr<PreSolveStage<ResourceType>> stage) {
            stages_.push_back(std::move(stage));
            stages_.back()->run(graph_, context_);
        }

        /// @brief Undoes every stage run so far, the last one first, and forgets them.
        void undo_all() noexcept {
            for (auto stage = stages_.rbegin(); stage != stages_.rend(); ++stage) {
                (*stage)->undo();
            }
            stages_.clear();
        }

    private:
        Graph<ResourceType>& graph_;
        SolveContext& context_;
        std::vector<std::unique_ptr<PreSolveStage<ResourceType>>> stages_;
};

}  // namespace rcspp
