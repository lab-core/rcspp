// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <cstddef>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

#include "rcspp/resource/concrete/functions/cost/value_cost_function.hpp"
#include "rcspp/resource/concrete/functions/dominance/inclusion_dominance_function.hpp"
#include "rcspp/resource/concrete/functions/dominance/value_dominance_function.hpp"
#include "rcspp/resource/concrete/functions/extension/addition_extension_function.hpp"
#include "rcspp/resource/concrete/functions/extension/budget_extension_function.hpp"
#include "rcspp/resource/concrete/functions/extension/ng-path_extension_function.hpp"
#include "rcspp/resource/concrete/functions/extension/time_window_extension_function.hpp"
#include "rcspp/resource/concrete/functions/feasibility/intersection_feasibility_function.hpp"
#include "rcspp/resource/concrete/functions/feasibility/min_max_feasibility_function.hpp"
#include "rcspp/resource/concrete/functions/feasibility/time_window_feasibility_function.hpp"
#include "rcspp/resource/functions/backward_kind.hpp"
#include "rcspp/resource/functions/cost/trivial_cost_function.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"
#include "rcspp/resource/functions/feasibility/trivial_feasibility_function.hpp"
#include "rcspp/resource/functions/node_bounds.hpp"

/// @file presets.hpp
/// @brief One call per resource kind, registering a coherent set of four function objects.
///
/// Presets are sugar over the four-object `add_resource` form; each one documents what it expands
/// to. Every preset asserts @c backward_coherent_v on its pairing, so an incoherent preset fails
/// to compile.

namespace rcspp {

/// @brief Whether an extension / feasibility pairing passes the backward-coherence checks that
///        its types can answer.
///
/// True when the extension declares a backward kind and an accumulating extension is not paired
/// with a feasibility function that seeds backward labels at a ceiling. Not enforced by
/// `add_resource`, since forward-only models need neither check:
///
/// @code
/// static_assert(rcspp::backward_coherent_v<MyExtension, MyFeasibility>);
/// @endcode
///
/// @tparam Ext  Concrete extension function type.
/// @tparam Feas Concrete feasibility function type.
template <typename Ext, typename Feas>
inline constexpr bool backward_coherent_v = backward_kind_of_v<Ext> != BackwardKind::Unspecified &&
                                            !(backward_kind_of_v<Ext> == BackwardKind::Accumulate &&
                                              back_seed_end_v<Feas> == BackSeedEnd::Ceiling);

}  // namespace rcspp

namespace rcspp::presets {

namespace detail {

/// @brief Registers a preset's four function objects after asserting that they are coherent.
///
/// @tparam R     The resource type.
/// @tparam Graph The graph type, deduced.
/// @param graph       The graph to register the resource on.
/// @param extension   The extension function.
/// @param feasibility The feasibility function.
/// @param cost        The cost function.
/// @param dominance   The dominance function.
template <typename R, typename Graph, typename Ext, typename Feas, typename Cost, typename Dom>
void add_coherent_resource(Graph& graph, std::unique_ptr<Ext> extension,
                           std::unique_ptr<Feas> feasibility, std::unique_ptr<Cost> cost,
                           std::unique_ptr<Dom> dominance) {
    static_assert(backward_kind_of_v<Ext> != BackwardKind::Unspecified,
                  "a preset's extension function declares no backward kind");
    static_assert(backward_coherent_v<Ext, Feas>,
                  "a preset pairs an accumulating extension with a feasibility function that "
                  "always seeds its backward label at a ceiling; a bounded accumulation is a "
                  "threshold -- use BudgetExtensionFunction");
    graph.template add_resource<R>(std::move(extension),
                                   std::move(feasibility),
                                   std::move(cost),
                                   std::move(dominance));
}

}  // namespace detail

/// @brief The cost slot: an unbounded accumulation whose value is the objective.
///
/// Expands to `add_resource(AdditionExtensionFunction, TrivialFeasibilityFunction,
/// ValueCostFunction, ValueDominanceFunction)`.
///
/// @tparam R     The resource type; normally a signed numerical type.
/// @tparam Graph The graph type, deduced.
/// @param graph The graph to register the resource on.
template <typename R, typename Graph>
void add_cost_resource(Graph& graph) {
    detail::add_coherent_resource<R>(graph,
                                     std::make_unique<AdditionExtensionFunction<R>>(),
                                     std::make_unique<TrivialFeasibilityFunction<R>>(),
                                     std::make_unique<ValueCostFunction<R>>(),
                                     std::make_unique<ValueDominanceFunction<R>>());
}

/// @brief A scalar resource with a per-node window (a threshold, seeded backward at its closing
///        bound).
///
/// Expands to `add_resource(TimeWindowExtensionFunction(windows), TimeWindowFeasibilityFunction(
/// windows), ValueCostFunction, ValueDominanceFunction)`, where `windows` is one
/// @ref SharedNodeBounds holding the map, with an opening time of 0 at the nodes absent from it.
///
/// @tparam R     The resource type. Must be signed: backward extension subtracts.
/// @tparam Graph The graph type, deduced.
/// @tparam V     The resource's arithmetic value type. Never deduced from the arguments, so an
///               `int` bound on a real resource converts rather than truncating every value.
/// @param graph       The graph to register the resource on.
/// @param windows     Per-node `{earliest, latest}` pairs.
/// @param default_max Bound used at nodes absent from the map.
template <typename R, typename Graph,
          typename V = std::decay_t<decltype(std::declval<R>().get_value())>>
void add_window_resource(
    Graph& graph,
    std::map<size_t, std::pair<std::type_identity_t<V>, std::type_identity_t<V>>> windows,
    std::type_identity_t<V> default_max = std::numeric_limits<V>::max() / 2) {
    const auto bounds = make_node_bounds(V{0}, default_max, std::move(windows));
    detail::add_coherent_resource<R>(graph,
                                     std::make_unique<TimeWindowExtensionFunction<R, V>>(bounds),
                                     std::make_unique<TimeWindowFeasibilityFunction<R, V>>(bounds),
                                     std::make_unique<ValueCostFunction<R>>(),
                                     std::make_unique<ValueDominanceFunction<R>>());
}

/// @brief @ref add_window_resource for integer windows on a floating-point resource, as
///        instances with integer time windows give them. Each bound converts exactly.
///
/// @tparam R     The resource type, with a floating-point value.
/// @tparam Graph The graph type, deduced.
/// @tparam W     The windows' integer type, deduced.
/// @param graph   The graph to register the resource on.
/// @param windows Per-node `{earliest, latest}` pairs.
template <typename R, typename Graph, std::integral W,
          typename V = std::decay_t<decltype(std::declval<R>().get_value())>>
    requires std::floating_point<V>
void add_window_resource(Graph& graph, const std::map<size_t, std::pair<W, W>>& windows) {
    std::map<size_t, std::pair<V, V>> converted;
    for (const auto& [node_id, window] : windows) {
        converted.emplace(
            node_id,
            std::pair<V, V>{static_cast<V>(window.first), static_cast<V>(window.second)});
    }
    add_window_resource<R, Graph, V>(graph, std::move(converted));
}

/// @brief A bounded accumulation (capacity, duration, any budget).
///
/// Expands to `add_resource(BudgetExtensionFunction(caps), MinMaxFeasibilityFunction(caps),
/// TrivialCostFunction, ValueDominanceFunction)`, where `caps` is one @ref SharedNodeBounds of
/// `[0, capacity]` with the per-node capacities as overrides.
///
/// Both functions read the same capacities: the feasibility function enforces them forward, and
/// the extension clamps backward labels to them. The minimum is 0, which a bidirectional solve
/// checks only while loads are non-negative, so they must be: setup refuses a negative arc load.
/// A non-zero minimum would go unchecked, since backward labels carry only a ceiling.
///
/// @tparam R     The resource type. Must be signed.
/// @tparam Graph The graph type, deduced.
/// @tparam V     The resource's arithmetic value type. Never deduced from the arguments, so a
///               capacity of `5` on a real resource converts to `5.0` rather than truncating
///               every consumption.
/// @param graph    The graph to register the resource on.
/// @param capacity The upper bound at every node without its own.
/// @param per_node Optional per-node capacities; empty means uniform.
template <typename R, typename Graph,
          typename V = std::decay_t<decltype(std::declval<R>().get_value())>>
void add_budget_resource(Graph& graph, std::type_identity_t<V> capacity,
                         const std::map<size_t, std::type_identity_t<V>>& per_node = {}) {
    std::map<size_t, std::pair<V, V>> windows;
    for (const auto& [node_id, node_capacity] : per_node) {
        windows.emplace(node_id, std::pair<V, V>{V{0}, node_capacity});
    }

    const auto caps = make_node_bounds(V{0}, capacity, std::move(windows));
    detail::add_coherent_resource<R>(graph,
                                     std::make_unique<BudgetExtensionFunction<R, V>>(caps),
                                     std::make_unique<MinMaxFeasibilityFunction<R, V>>(caps),
                                     std::make_unique<TrivialCostFunction<R>>(),
                                     std::make_unique<ValueDominanceFunction<R>>());
}

/// @brief The ng-path relaxation: an endpoint mirror plus the ng-route condition.
///
/// Expands to `add_resource(NgPathExtensionFunction(neighborhoods),
/// IntersectionFeasibilityFunction({v: {v}}, forbidden = true), TrivialCostFunction,
/// InclusionDominanceFunction)`. The forbidden set `{v}` is derived for every node that
/// @p neighborhoods mentions, as a key or a member.
///
/// @tparam R     The container resource type.
/// @tparam Graph The graph type, deduced.
/// @tparam E     The element type stored in the sets.
/// @param graph         The graph to register the resource on.
/// @param neighborhoods Each node's ng-neighborhood. A node absent from the map forgets every
///                      previously visited node on arrival, so give every node a neighborhood.
template <typename R, typename Graph, typename E = typename R::ValueType>
void add_ng_path_resource(Graph& graph, std::map<size_t, std::set<E>> neighborhoods) {
    std::map<size_t, std::set<E>> forbidden_by_node;
    for (const auto& [node_id, neighborhood] : neighborhoods) {
        forbidden_by_node[node_id] = {static_cast<E>(node_id)};
        for (const E& member : neighborhood) {
            forbidden_by_node[static_cast<size_t>(member)] = {member};
        }
    }

    auto extension = std::make_unique<NgPathExtensionFunction<R, E>>(std::move(neighborhoods));
    auto feasibility =
        std::make_unique<IntersectionFeasibilityFunction<R, E>>(std::move(forbidden_by_node),
                                                                /*forbidden=*/true);

    detail::add_coherent_resource<R>(graph,
                                     std::move(extension),
                                     std::move(feasibility),
                                     std::make_unique<TrivialCostFunction<R>>(),
                                     std::make_unique<InclusionDominanceFunction<R>>());
}

/// @brief An elementary path: no node may be visited twice.
///
/// Expands to @ref add_ng_path_resource with every node's neighborhood set to every node. Use this
/// rather than a `UnionExtensionFunction` visited set, which is incoherent backward (each backward
/// label would contain its own node and be rejected immediately).
///
/// @note The shared neighborhood map totals `O(n^2)` elements, but each arc's extension caches
///       both endpoints' neighborhoods, every node under this preset, so `O(n^3)` in total. Use a
///       bitset memory such as `SizeTBitsetResource`: on a complete graph with 200 nodes the
///       caches take 15 MB as bitsets and about 900 MB as `SetResource`.
///
/// @tparam R     The container resource type.
/// @tparam Graph The graph type, deduced.
/// @tparam E     The element type stored in the sets.
/// @param graph    The graph to register the resource on.
/// @param node_ids Every node id in the model (resources are registered before nodes). A node left
///                 out gets no neighborhood, and elementarity stops holding through it.
template <typename R, typename Graph, typename E = typename R::ValueType>
void add_elementary_resource(Graph& graph, const std::vector<size_t>& node_ids) {
    std::set<E> every_node;
    for (const size_t node_id : node_ids) {
        every_node.insert(static_cast<E>(node_id));
    }

    std::map<size_t, std::set<E>> neighborhoods;
    for (const size_t node_id : node_ids) {
        neighborhoods[node_id] = every_node;
    }

    add_ng_path_resource<R, Graph, E>(graph, std::move(neighborhoods));
}

}  // namespace rcspp::presets
