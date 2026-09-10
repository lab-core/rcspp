// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <map>
#include <memory>
#include <set>
#include <type_traits>
#include <utility>

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
#include "rcspp/resource/functions/cost/trivial_cost_function.hpp"
#include "rcspp/resource/functions/feasibility/trivial_feasibility_function.hpp"

/// @file presets.hpp
/// @brief One call per resource *kind*, for the four shapes this library actually has.
///
/// A resource is four function objects that have to agree with each other. Individually each can
/// be legal while the quadruple is incoherent -- the recorded example is an accumulating
/// extension paired with a feasibility function that seeds its backward label at a ceiling, which
/// is coherent component by component and silently finds nothing backward. A preset makes that
/// pairing unwritable.
///
/// **The four-object `add_resource` form remains normative.** These are sugar over it: each
/// preset's doc comment names exactly what it expands to, so stepping down to the general form is
/// obvious the moment a model needs something a preset does not offer (a flipped dominance, a
/// non-trivial cost on a budget, per-node min/max overrides). Presets serve the common case and
/// deliberately do not chase constructor parity.
///
/// Presets call `add_resource` with inline `std::make_unique`, so they bind the *typed* overload
/// and inherit its `static_assert`s -- an incoherent preset would fail to compile, which is the
/// right place for that failure, since a preset is the thing claiming coherence.

namespace rcspp::presets {

/// @brief The cost slot: an unbounded accumulation whose value *is* the objective.
///
/// Expands to `add_resource(AdditionExtensionFunction, TrivialFeasibilityFunction,
/// ValueCostFunction, ValueDominanceFunction)`. Use the four-object form directly if you need a
/// non-trivial feasibility or a flipped dominance.
///
/// @tparam R     The resource type; a signed numerical type in every current use.
/// @tparam Graph The graph type, deduced.
/// @param graph The graph to register the resource on.
template <typename R, typename Graph>
void add_cost_resource(Graph& graph) {
    graph.template add_resource<R>(std::make_unique<AdditionExtensionFunction<R>>(),
                                   std::make_unique<TrivialFeasibilityFunction<R>>(),
                                   std::make_unique<ValueCostFunction<R>>(),
                                   std::make_unique<ValueDominanceFunction<R>>());
}

/// @brief A scalar resource with a per-node window: a *threshold*, seeded backward at its closing
///        bound.
///
/// Expands to `add_resource(TimeWindowExtensionFunction, TimeWindowFeasibilityFunction,
/// ValueCostFunction, ValueDominanceFunction)`.
///
/// The window map is passed **once**. Building this by hand requires giving the same map to both
/// the extension and the feasibility function, which both of those headers note as a wart; a
/// preset is the first place it can be fixed for the caller.
///
/// @tparam R     The resource type. Must be signed: backward extension subtracts.
/// @tparam Graph The graph type, deduced.
/// @tparam V     The resource's arithmetic value type.
/// @param graph       The graph to register the resource on.
/// @param windows     Per-node `{earliest, latest}` pairs.
/// @param default_max Bound used at nodes absent from the map.
template <typename R, typename Graph,
          typename V = std::decay_t<decltype(std::declval<R>().get_value())>>
void add_window_resource(Graph& graph, std::map<size_t, std::pair<V, V>> windows,
                         V default_max = std::numeric_limits<V>::max() / 2) {
    // Named locals, NOT inline in the call. Function arguments are unsequenced relative to one
    // another, so an inline `windows` / `std::move(windows)` pair may evaluate the move first --
    // g++ typically evaluates right to left -- leaving the *extension* function with an empty
    // window map. That failure is quiet: feasibility still has the real map, so the model does
    // not throw, it just stops clamping and returns wrong paths.
    auto extension = std::make_unique<TimeWindowExtensionFunction<R, V>>(windows, default_max);
    auto feasibility =
        std::make_unique<TimeWindowFeasibilityFunction<R, V>>(std::move(windows), default_max);

    graph.template add_resource<R>(std::move(extension),
                                   std::move(feasibility),
                                   std::make_unique<ValueCostFunction<R>>(),
                                   std::make_unique<ValueDominanceFunction<R>>());
}

/// @brief A bounded accumulation -- capacity, duration, any budget.
///
/// Expands to `add_resource(BudgetExtensionFunction, MinMaxFeasibilityFunction(min, capacity,
/// true), TrivialCostFunction, ValueDominanceFunction)`.
///
/// This is the preset that exists because the mistake it prevents actually happened. Pairing
/// @c AdditionExtensionFunction with @c MinMaxFeasibilityFunction gives a backward label that
/// starts at the ceiling while the extension *adds*, so it leaves its range on the first arc and
/// the backward search silently finds nothing. A bounded accumulation is a **threshold**, which
/// is what @c BudgetExtensionFunction is for.
///
/// @tparam R     The resource type. Must be signed.
/// @tparam Graph The graph type, deduced.
/// @tparam V     The resource's arithmetic value type.
/// @param graph    The graph to register the resource on.
/// @param capacity The global upper bound.
/// @param per_node Optional per-node capacities; empty means uniform.
/// @param minimum  The global lower bound, normally zero.
template <typename R, typename Graph,
          typename V = std::decay_t<decltype(std::declval<R>().get_value())>>
void add_budget_resource(Graph& graph, V capacity, std::map<size_t, V> per_node = {},
                         V minimum = V{0}) {
    // `merge_by_increasing_value = true` is named rather than defaulted: the two-argument
    // MinMaxFeasibilityFunction constructor leaves it true anyway, and relying on that default is
    // what made the original defect hard to see.
    graph.template add_resource<R>(
        std::make_unique<BudgetExtensionFunction<R, V>>(std::move(per_node), capacity),
        std::make_unique<MinMaxFeasibilityFunction<R, V>>(minimum,
                                                          capacity,
                                                          /*merge_by_increasing_value=*/true),
        std::make_unique<TrivialCostFunction<R>>(),
        std::make_unique<ValueDominanceFunction<R>>());
}

/// @brief The ng-path relaxation: a node-identity mirror plus a forbidden-set feasibility.
///
/// Expands to `add_resource(NgPathExtensionFunction, IntersectionFeasibilityFunction(forbidden),
/// TrivialCostFunction, InclusionDominanceFunction)`.
///
/// For the usual ng-route condition pass `forbidden_by_node[v] = {v}`: a label arriving at `v` is
/// infeasible when `v` is already in the memory its own half accumulated.
///
/// @tparam R     The container resource type.
/// @tparam Graph The graph type, deduced.
/// @tparam E     The element type stored in the sets.
/// @param graph             The graph to register the resource on.
/// @param neighborhoods     Each node's ng-neighborhood; absent means no narrowing.
/// @param forbidden_by_node The set forbidden at each node.
template <typename R, typename Graph, typename E = typename R::ValueType>
void add_ng_path_resource(Graph& graph, std::map<size_t, std::set<E>> neighborhoods,
                          std::map<size_t, std::set<E>> forbidden_by_node) {
    // Named locals, per the warning in add_window_resource. These are two different maps, but the
    // habit is what keeps the next preset safe.
    auto extension = std::make_unique<NgPathExtensionFunction<R, E>>(std::move(neighborhoods));
    auto feasibility =
        std::make_unique<IntersectionFeasibilityFunction<R, E>>(std::move(forbidden_by_node),
                                                                /*forbidden=*/true);

    graph.template add_resource<R>(std::move(extension),
                                   std::move(feasibility),
                                   std::make_unique<TrivialCostFunction<R>>(),
                                   std::make_unique<InclusionDominanceFunction<R>>());
}

}  // namespace rcspp::presets
