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
#include "rcspp/resource/concrete/functions/extension/capacity_extension_function.hpp"
#include "rcspp/resource/concrete/functions/extension/ng-path_extension_function.hpp"
#include "rcspp/resource/concrete/functions/extension/time_window_extension_function.hpp"
#include "rcspp/resource/concrete/functions/feasibility/intersection_feasibility_function.hpp"
#include "rcspp/resource/concrete/functions/feasibility/min_max_feasibility_function.hpp"
#include "rcspp/resource/concrete/functions/feasibility/time_window_feasibility_function.hpp"
#include "rcspp/resource/functions/cost/trivial_cost_function.hpp"
#include "rcspp/resource/functions/directional_resource_kind.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"
#include "rcspp/resource/functions/feasibility/trivial_feasibility_function.hpp"
#include "rcspp/resource/functions/node_bounds.hpp"

/// @file presets.hpp
/// @brief One call per resource kind, registering a coherent set of four function objects.
///
/// Presets are sugar over the four-object `add_resource` form; each one documents what it expands
/// to. Every preset asserts @c declares_directional_resource_kind_v on its extension, so a preset
/// whose extension has no backward reading fails to compile. Whether the rest of the quadruple fits
/// depends on its settings, which the model checks test before a backward or bidirectional solve.

namespace rcspp::presets {

namespace detail {

/// @brief Registers a preset's four function objects after asserting that they are coherent.
///
/// @tparam ResourceType The resource type.
/// @tparam Graph        The graph type, deduced.
/// @param graph       The graph to register the resource on.
/// @param extension   The extension function.
/// @param feasibility The feasibility function.
/// @param cost        The cost function.
/// @param dominance   The dominance function.
template <typename ResourceType, typename Graph, typename ExtensionFunctionType,
          typename FeasibilityFunctionType, typename CostFunctionType,
          typename DominanceFunctionType>
void add_coherent_resource(Graph& graph, std::unique_ptr<ExtensionFunctionType> extension,
                           std::unique_ptr<FeasibilityFunctionType> feasibility,
                           std::unique_ptr<CostFunctionType> cost,
                           std::unique_ptr<DominanceFunctionType> dominance) {
    static_assert(directional_resource_kind_of_v<ExtensionFunctionType> !=
                      DirectionalResourceKind::Unspecified,
                  "a preset's extension function declares no directional resource kind");
    graph.template add_resource<ResourceType>(std::move(extension),
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
/// @tparam ResourceType The resource type; normally a signed numerical type.
/// @tparam Graph        The graph type, deduced.
/// @param graph The graph to register the resource on.
template <typename ResourceType, typename Graph>
void add_cost_resource(Graph& graph) {
    detail::add_coherent_resource<ResourceType>(
        graph,
        std::make_unique<AdditionExtensionFunction<ResourceType>>(),
        std::make_unique<TrivialFeasibilityFunction<ResourceType>>(),
        std::make_unique<ValueCostFunction<ResourceType>>(),
        std::make_unique<ValueDominanceFunction<ResourceType>>());
}

/// @brief A scalar resource with a per-node window (a threshold, whose backward labels start at
///        its closing bound).
///
/// Expands to `add_resource(TimeWindowExtensionFunction(windows), TimeWindowFeasibilityFunction(
/// windows), TrivialCostFunction, ValueDominanceFunction)`, where `windows` is one
/// @ref SharedNodeBounds holding the map, with an opening time of 0 at the nodes absent from it.
///
/// The window costs nothing, as a capacity does: its backward value is a deadline, not a cost, so
/// a cost that read it could not be split between two halves, and a `CompositionCostFunction` would
/// add arrival times to the objective. A model whose objective is the arrival time uses the
/// four-object form.
///
/// @tparam ResourceType The resource type. Must be signed: backward extension subtracts.
/// @tparam Graph        The graph type, deduced.
/// @tparam ValueType    The resource's arithmetic value type. Never deduced from the arguments, so
///                      an `int` bound on a real resource converts rather than truncating every
///                      value.
/// @param graph       The graph to register the resource on.
/// @param windows     Per-node `{earliest, latest}` pairs.
/// @param default_max Bound used at nodes absent from the map.
template <typename ResourceType, typename Graph,
          typename ValueType = std::decay_t<decltype(std::declval<ResourceType>().get_value())>>
void add_window_resource(
    Graph& graph,
    std::map<size_t, std::pair<std::type_identity_t<ValueType>, std::type_identity_t<ValueType>>>
        windows,
    std::type_identity_t<ValueType> default_max = std::numeric_limits<ValueType>::max() / 2) {
    const auto bounds = make_node_bounds(ValueType{0}, default_max, std::move(windows));
    detail::add_coherent_resource<ResourceType>(
        graph,
        std::make_unique<TimeWindowExtensionFunction<ResourceType, ValueType>>(bounds),
        std::make_unique<TimeWindowFeasibilityFunction<ResourceType, ValueType>>(bounds),
        std::make_unique<TrivialCostFunction<ResourceType>>(),
        std::make_unique<ValueDominanceFunction<ResourceType>>());
}

/// @brief @ref add_window_resource for integer windows on a floating-point resource, as
///        instances with integer time windows give them. Each bound converts exactly.
///
/// @tparam ResourceType    The resource type, with a floating-point value.
/// @tparam Graph           The graph type, deduced.
/// @tparam WindowValueType The windows' integer type, deduced.
/// @param graph   The graph to register the resource on.
/// @param windows Per-node `{earliest, latest}` pairs.
template <typename ResourceType, typename Graph, std::integral WindowValueType,
          typename ValueType = std::decay_t<decltype(std::declval<ResourceType>().get_value())>>
    requires std::floating_point<ValueType>
void add_window_resource(
    Graph& graph, const std::map<size_t, std::pair<WindowValueType, WindowValueType>>& windows) {
    std::map<size_t, std::pair<ValueType, ValueType>> converted;
    for (const auto& [node_id, window] : windows) {
        converted.emplace(node_id,
                          std::pair<ValueType, ValueType>{static_cast<ValueType>(window.first),
                                                          static_cast<ValueType>(window.second)});
    }
    add_window_resource<ResourceType, Graph, ValueType>(graph, std::move(converted));
}

/// @brief A bounded accumulation (capacity, duration, any limit).
///
/// Expands to `add_resource(CapacityExtensionFunction(caps), MinMaxFeasibilityFunction(caps),
/// TrivialCostFunction, ValueDominanceFunction)`, where `caps` is one @ref SharedNodeBounds of
/// `[0, capacity]` with the per-node capacities as overrides.
///
/// Both functions read the same capacities: the feasibility function enforces them forward, and
/// the extension clamps backward labels to them. The minimum is fixed at 0. Backward labels carry
/// only an upper limit, so a bidirectional solve cannot check a minimum; 0 holds automatically as
/// long as every arc load is non-negative, and the model checks refuse a negative one.
///
/// @tparam ResourceType The resource type. Must be signed.
/// @tparam Graph        The graph type, deduced.
/// @tparam ValueType    The resource's arithmetic value type. Never deduced from the arguments, so
///                      a capacity of `5` on a real resource converts to `5.0` rather than
///                      truncating every consumption.
/// @param graph    The graph to register the resource on.
/// @param capacity The upper bound at every node without its own.
/// @param per_node Optional per-node capacities; empty means uniform.
template <typename ResourceType, typename Graph,
          typename ValueType = std::decay_t<decltype(std::declval<ResourceType>().get_value())>>
void add_capacity_resource(Graph& graph, std::type_identity_t<ValueType> capacity,
                           const std::map<size_t, std::type_identity_t<ValueType>>& per_node = {}) {
    std::map<size_t, std::pair<ValueType, ValueType>> windows;
    for (const auto& [node_id, node_capacity] : per_node) {
        windows.emplace(node_id, std::pair<ValueType, ValueType>{ValueType{0}, node_capacity});
    }

    const auto caps = make_node_bounds(ValueType{0}, capacity, std::move(windows));
    detail::add_coherent_resource<ResourceType>(
        graph,
        std::make_unique<CapacityExtensionFunction<ResourceType, ValueType>>(caps),
        std::make_unique<MinMaxFeasibilityFunction<ResourceType, ValueType>>(caps),
        std::make_unique<TrivialCostFunction<ResourceType>>(),
        std::make_unique<ValueDominanceFunction<ResourceType>>());
}

/// @brief The ng-path relaxation: a memory read from the arc's endpoints, plus the ng-route
/// condition.
///
/// Expands to `add_resource(NgPathExtensionFunction(neighborhoods),
/// IntersectionFeasibilityFunction({v: {v}}, forbidden = true), TrivialCostFunction,
/// InclusionDominanceFunction)`. The forbidden set `{v}` is derived for every node that
/// @p neighborhoods mentions, as a key or a member.
///
/// @tparam ResourceType The container resource type.
/// @tparam Graph        The graph type, deduced.
/// @tparam ValueType    The element type stored in the sets.
/// @param graph         The graph to register the resource on.
/// @param neighborhoods Each node's ng-neighborhood. A node absent from the map forgets every
///                      previously visited node on arrival, so give every node a neighborhood.
template <typename ResourceType, typename Graph,
          typename ValueType = typename ResourceType::ValueType>
void add_ng_path_resource(Graph& graph, std::map<size_t, std::set<ValueType>> neighborhoods) {
    std::map<size_t, std::set<ValueType>> forbidden_by_node;
    for (const auto& [node_id, neighborhood] : neighborhoods) {
        forbidden_by_node[node_id] = {static_cast<ValueType>(node_id)};
        for (const ValueType& member : neighborhood) {
            forbidden_by_node[static_cast<size_t>(member)] = {member};
        }
    }

    auto extension = std::make_unique<NgPathExtensionFunction<ResourceType, ValueType>>(
        std::move(neighborhoods));
    auto feasibility = std::make_unique<IntersectionFeasibilityFunction<ResourceType, ValueType>>(
        std::move(forbidden_by_node),
        /*forbidden=*/true);

    detail::add_coherent_resource<ResourceType>(
        graph,
        std::move(extension),
        std::move(feasibility),
        std::make_unique<TrivialCostFunction<ResourceType>>(),
        std::make_unique<InclusionDominanceFunction<ResourceType>>());
}

/// @brief An elementary path: no node may be visited twice.
///
/// Expands to @ref add_ng_path_resource with every node's neighborhood set to every node. Use this
/// rather than a `UnionExtensionFunction` visited set, which is incoherent backward (each backward
/// label would contain its own node and be rejected immediately).
///
/// @note Every node's neighborhood holds every node, `O(n^2)` elements in total, kept once per
///       node and shared by the arcs. Use a bitset memory such as `SizeTBitsetResource`, which
///       stores each set in `n` bits rather than one tree node per element.
///
/// @tparam ResourceType The container resource type.
/// @tparam Graph        The graph type, deduced.
/// @tparam ValueType    The element type stored in the sets.
/// @param graph    The graph to register the resource on.
/// @param node_ids Every node id in the model (resources are registered before nodes). A node left
///                 out gets no neighborhood, and elementarity stops holding through it.
template <typename ResourceType, typename Graph,
          typename ValueType = typename ResourceType::ValueType>
void add_elementary_resource(Graph& graph, const std::vector<size_t>& node_ids) {
    std::set<ValueType> every_node;
    for (const size_t node_id : node_ids) {
        every_node.insert(static_cast<ValueType>(node_id));
    }

    std::map<size_t, std::set<ValueType>> neighborhoods;
    for (const size_t node_id : node_ids) {
        neighborhoods[node_id] = every_node;
    }

    add_ng_path_resource<ResourceType, Graph, ValueType>(graph, std::move(neighborhoods));
}

}  // namespace rcspp::presets
