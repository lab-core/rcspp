// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "rcspp/algorithm/solution.hpp"
#include "rcspp/graph/graph.hpp"

namespace rcspp {

/// @brief The default relative tolerance of @ref PathEvaluator on a solution's cost.
inline constexpr double kDefaultPathCostTolerance = 1e-6;

/// @brief What is wrong with a path: the first fault a @ref PathEvaluator finds.
///
/// No fault reports a source or a sink inside a path: the graph refuses an arc that enters a
/// source or leaves a sink, so a connected path from a source to a sink has neither inside it.
enum class PathFault {
    None,           ///< nothing: the path is valid
    Empty,          ///< the path has no arc
    UnknownArc,     ///< an arc id names no arc of the graph, or one removed from it
    Disconnected,   ///< an arc does not start where the arc before it ends
    NotFromSource,  ///< the first arc does not leave a source
    NotToSink,      ///< the last arc does not enter a sink
    Unreachable,    ///< the look-ahead refuses an arc: its destination cannot be reached
    Infeasible,     ///< the label at an arc's destination is infeasible
    NodesDisagree,  ///< a solution's node ids are not the nodes its arcs visit
    CostDisagrees,  ///< a solution's cost is not the cost its path replays at
};

/// @brief What a @ref PathEvaluator found on a path.
struct PathEvaluation {
        /// @brief The first fault found, or @c PathFault::None.
        PathFault fault = PathFault::None;
        /// @brief Where: the index of the arc at fault in the path, or of the first node id that
        ///        disagrees; 0 for an empty path and for a cost that disagrees.
        size_t position = 0;
        /// @brief The cost the path replays at; +infinity when the replay stopped before its end.
        double cost = std::numeric_limits<double>::infinity();

        /// @brief Whether no fault was found.
        [[nodiscard]] bool is_valid() const { return fault == PathFault::None; }
};

/// @brief Replays a path through a graph's resources, as a forward search extends a label, and
///        says whether the path is one the graph allows, and what it costs.
///
/// The replay starts from a fresh label at the path's source and extends it along each arc in
/// turn: the look-ahead (`is_reachable`) on the arc's destination, the arc's extension into a
/// fresh label at the destination, and its feasibility. It shares nothing with the searches but
/// the model's own functions, so it checks what any search returns, a heuristic's included.
///
/// Evaluate on the graph the solve ran on, after the solve returned: a solve that preprocesses
/// restores the arcs it removed before it returns, and a solution's arc ids are the graph's.
///
/// @tparam ResourceType The graph's resource type.
template <typename ResourceType>
class PathEvaluator {
    public:
        /// @brief An evaluator of paths through @p graph, which must outlive it.
        explicit PathEvaluator(const Graph<ResourceType>& graph) : graph_(&graph) {}

        /// @brief Evaluates the path through the arcs @p arc_ids, in order.
        ///
        /// The checks, in order, the first that fails reported: the path has an arc; every id
        /// names an arc of the graph; consecutive arcs meet; the path leaves a source and enters
        /// a sink; replayed, every arc passes the look-ahead and every label is feasible.
        ///
        /// @param arc_ids The path's arc ids.
        /// @return The first fault, and the path's cost when it has none.
        [[nodiscard]] PathEvaluation evaluate(const std::vector<size_t>& arc_ids) const {
            std::vector<const Arc<ResourceType>*> arcs;
            if (auto fault = find_arcs(arc_ids, &arcs); !fault.is_valid()) {
                return fault;
            }
            return replay(arcs);
        }

        /// @brief Evaluates the path of @p solution, and checks its node ids and its cost.
        ///
        /// After the checks of @ref evaluate(const std::vector<size_t>&) on its arc ids: its
        /// node ids, when it has any, are the nodes the arcs visit; its cost is the replayed
        /// cost, within @p tolerance (relative to the cost's magnitude, absolute below 1).
        ///
        /// @param solution  The solution to check.
        /// @param tolerance The allowed difference between the two costs.
        /// @return The first fault, and the replayed cost.
        [[nodiscard]] PathEvaluation evaluate(const Solution& solution,
                                              double tolerance = kDefaultPathCostTolerance) const {
            std::vector<const Arc<ResourceType>*> arcs;
            if (auto fault = find_arcs(solution.path_arc_ids, &arcs); !fault.is_valid()) {
                return fault;
            }
            if (!solution.path_node_ids.empty()) {
                if (auto fault = check_nodes(arcs, solution.path_node_ids); !fault.is_valid()) {
                    return fault;
                }
            }
            PathEvaluation evaluation = replay(arcs);
            if (!evaluation.is_valid()) {
                return evaluation;
            }
            const double scale = std::max(1.0, std::abs(evaluation.cost));
            if (!(std::abs(evaluation.cost - solution.cost) <= tolerance * scale)) {
                evaluation.fault = PathFault::CostDisagrees;
            }
            return evaluation;
        }

    private:
        /// @brief Looks up the arcs of @p arc_ids into @p arcs, and checks that they form a path
        ///        from a source to a sink.
        PathEvaluation find_arcs(const std::vector<size_t>& arc_ids,
                                 std::vector<const Arc<ResourceType>*>* arcs) const {
            if (arc_ids.empty()) {
                return {.fault = PathFault::Empty};
            }
            arcs->reserve(arc_ids.size());
            for (size_t i = 0; i < arc_ids.size(); ++i) {
                const Arc<ResourceType>* arc = graph_->get_arc(arc_ids[i]);
                if (arc == nullptr) {
                    return {.fault = PathFault::UnknownArc, .position = i};
                }
                if (i > 0 && arcs->back()->destination != arc->origin) {
                    return {.fault = PathFault::Disconnected, .position = i};
                }
                arcs->push_back(arc);
            }
            if (!arcs->front()->origin->source) {
                return {.fault = PathFault::NotFromSource};
            }
            if (!arcs->back()->destination->sink) {
                return {.fault = PathFault::NotToSink, .position = arcs->size() - 1};
            }
            return {};
        }

        /// @brief Checks that @p node_ids are the nodes @p arcs visit.
        static PathEvaluation check_nodes(const std::vector<const Arc<ResourceType>*>& arcs,
                                          const std::vector<size_t>& node_ids) {
            for (size_t i = 0; i <= arcs.size(); ++i) {
                const size_t visited =
                    i < arcs.size() ? arcs[i]->origin->id : arcs.back()->destination->id;
                if (i >= node_ids.size() || node_ids[i] != visited) {
                    return {.fault = PathFault::NodesDisagree, .position = i};
                }
            }
            if (node_ids.size() != arcs.size() + 1) {
                return {.fault = PathFault::NodesDisagree, .position = arcs.size() + 1};
            }
            return {};
        }

        /// @brief Replays @p arcs, a path from a source to a sink, from a fresh label at its
        ///        source.
        static PathEvaluation replay(const std::vector<const Arc<ResourceType>*>& arcs) {
            auto current = arcs.front()->origin->resource->copy();
            for (size_t i = 0; i < arcs.size(); ++i) {
                const Arc<ResourceType>& arc = *arcs[i];
                if (!current->is_reachable(arc.destination->id)) {
                    return {.fault = PathFault::Unreachable, .position = i};
                }
                auto next = arc.destination->resource->copy();
                arc.extender->extend(*current, next.get());
                if (!next->is_feasible()) {
                    return {.fault = PathFault::Infeasible, .position = i};
                }
                current = std::move(next);
            }
            return {.cost = current->get_cost()};
        }

        const Graph<ResourceType>* graph_;
};

}  // namespace rcspp
