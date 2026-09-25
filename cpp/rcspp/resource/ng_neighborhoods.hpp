// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "rcspp/resource/composition/composition.hpp"
#include "rcspp/resource/concrete/functions/extension/ng-path_extension_function.hpp"
#include "rcspp/resource/resource_graph.hpp"

namespace rcspp {

/// @brief One revisit in a path: node @ref node seen at positions @ref first and @ref last, with
///        no visit to it in between.
struct NgCycle {
        size_t node = 0;               ///< The node visited twice.
        size_t first = 0;              ///< Position of the earlier visit in the sequence.
        size_t last = 0;               ///< Position of the later visit.
        std::vector<size_t> interior;  ///< The nodes strictly between the two visits, in order.

        /// @brief Nodes strictly inside the cycle: the size ng-growth rules are expressed in.
        [[nodiscard]] size_t length() const { return interior.size(); }
};

/// @brief Every pair of consecutive visits to the same node in @p sequence.
///
/// A node visited three times yields two cycles, one per consecutive pair. Cycles are ordered by
/// the position of their later visit.
///
/// @param sequence Node ids in traversal order.
/// @return The cycles; empty for an elementary sequence.
[[nodiscard]] inline std::vector<NgCycle> find_cycles(std::span<const size_t> sequence) {
    std::vector<NgCycle> cycles;
    std::unordered_map<size_t, size_t> last_seen;
    for (size_t position = 0; position < sequence.size(); ++position) {
        const size_t node = sequence[position];
        if (auto it = last_seen.find(node); it != last_seen.end()) {
            NgCycle cycle{.node = node, .first = it->second, .last = position, .interior = {}};
            cycle.interior.assign(sequence.begin() + static_cast<std::ptrdiff_t>(it->second) + 1,
                                  sequence.begin() + static_cast<std::ptrdiff_t>(position));
            cycles.push_back(std::move(cycle));
        }
        last_seen[node] = position;
    }
    return cycles;
}

/// @brief Whether @p sequence is feasible under the ng-route relaxation with @p neighborhoods.
///
/// Replays `NgPathExtensionFunction` and the ng presets' `IntersectionFeasibilityFunction` exactly.
/// On the arc `a -> b` the memory becomes `(memory u {a}) n (ng(b) u {b})`. Arriving at a node the
/// table names is infeasible when that node is then in the memory: the forbidden set `{b}` the
/// presets derive for every node they are given. A node absent from the table narrows the memory to
/// `{itself}` and has no forbidden set, so it is never infeasible. A column-generation driver uses
/// this to find the master columns a grown memory has made infeasible.
///
/// @tparam E Element type of the neighbourhood sets.
/// @param sequence      Node ids in traversal order.
/// @param neighborhoods Each node's ng-neighbourhood.
/// @return @c true when no step revisits a node its memory still holds.
template <typename E>
[[nodiscard]] bool is_ng_feasible(std::span<const size_t> sequence,
                                  const std::map<size_t, std::set<E>>& neighborhoods) {
    std::set<E> memory;
    for (size_t i = 0; i + 1 < sequence.size(); ++i) {
        const auto arrived = static_cast<E>(sequence[i + 1]);
        memory.insert(static_cast<E>(sequence[i]));
        std::set<E> narrowed;
        const auto it = neighborhoods.find(sequence[i + 1]);
        for (const E& kept : memory) {
            if (kept == arrived || (it != neighborhoods.end() && it->second.contains(kept))) {
                narrowed.insert(kept);
            }
        }
        if (it != neighborhoods.end() && narrowed.contains(arrived)) {
            return false;
        }
        memory = std::move(narrowed);
    }
    return true;
}

/// @brief Checks that @p next can replace @p current without touching the feasibility functions.
///
/// The ng presets derive `forbidden(v) = {v}` once, at registration, for every node the table
/// names. So a new table must name the same nodes as keys, and every member must be one of them.
/// Growing or shrinking a member set within those nodes is what augmentation does, and is always
/// allowed.
///
/// @tparam E Element type of the neighbourhood sets.
/// @param current The table the resource uses now.
/// @param next    The table to replace it with.
/// @throws std::invalid_argument naming the first offending node.
template <typename E>
void check_ng_update(const std::map<size_t, std::set<E>>& current,
                     const std::map<size_t, std::set<E>>& next) {
    for (const auto& [node_id, members] : next) {
        if (!current.contains(node_id)) {
            throw std::invalid_argument(
                "set_ng_neighborhoods: node " + std::to_string(node_id) +
                " has no neighbourhood in the table the resource was registered with; the keys are "
                "fixed, because the feasibility functions forbid exactly those nodes");
        }
        for (const E& member : members) {
            if (!current.contains(static_cast<size_t>(member))) {
                throw std::invalid_argument(
                    "set_ng_neighborhoods: the neighbourhood of node " + std::to_string(node_id) +
                    " names node " + std::to_string(static_cast<size_t>(member)) +
                    ", which the registered table does not; only registered nodes can be members");
            }
        }
    }
    for (const auto& [node_id, members] : current) {
        if (!next.contains(node_id)) {
            throw std::invalid_argument("set_ng_neighborhoods: the new table drops node " +
                                        std::to_string(node_id) +
                                        "; give it an empty neighbourhood instead");
        }
    }
}

namespace ng_detail {

/// @brief The type-slot index of @p R in a `ResourceGraph`'s resource types.
template <typename Graph>
struct resource_types;

template <typename... Ts>
struct resource_types<ResourceGraph<Ts...>> {
        template <typename R>
        static constexpr size_t index_of = static_cast<size_t>(ComponentTypeIndex_v<R, Ts...>);
};

/// @brief The factory of component @p component_index of type @p R, and its ng prototype.
///
/// @throws std::out_of_range     when there is no such component.
/// @throws std::invalid_argument when that component is not an ng-path resource.
template <typename R, typename E, typename Graph>
auto ng_factory(Graph& graph, size_t component_index)
    -> std::pair<ResourceFactory<R>*, NgPathExtensionFunction<R, E>*> {
    constexpr size_t kIndex = resource_types<Graph>::template index_of<R>;
    auto& factories = graph.get_resource_factory().template get_components<kIndex>();
    if (component_index >= factories.size()) {
        throw std::out_of_range("set_ng_neighborhoods: component " +
                                std::to_string(component_index) +
                                " does not exist: the model has " +
                                std::to_string(factories.size()) + " of that resource type");
    }
    auto* factory = factories[component_index].get();
    auto* ng = dynamic_cast<NgPathExtensionFunction<R, E>*>(&factory->extension_function());
    if (ng == nullptr) {
        throw std::invalid_argument("set_ng_neighborhoods: component " +
                                    std::to_string(component_index) +
                                    " is not an ng-path resource (NgPathExtensionFunction)");
    }
    return {factory, ng};
}

}  // namespace ng_detail

/// @brief The neighbourhoods an ng-path resource currently uses.
///
/// @tparam R     The container resource type the ng memory is stored in.
/// @tparam E     Element type of the sets.
/// @tparam Graph The `ResourceGraph` type, deduced.
/// @param graph           The graph.
/// @param component_index Which component of type @p R is the ng memory (0 for the first).
/// @return The table, as last given at registration or to @ref set_ng_neighborhoods.
/// @throws std::out_of_range, std::invalid_argument as @ref set_ng_neighborhoods.
template <typename R, typename E = typename R::ValueType, typename Graph>
[[nodiscard]] const std::map<size_t, std::set<E>>& ng_neighborhoods(Graph& graph,
                                                                    size_t component_index) {
    return ng_detail::ng_factory<R, E>(graph, component_index).second->neighborhoods();
}

/// @brief Replaces an ng-path resource's neighbourhoods on a graph already built, between solves.
///
/// Arcs cache the neighbourhoods they read when they are created (`EndpointMirrorForm` caches both
/// of an arc's sides in `preprocess`), so replacing the table alone would change no arc. This
/// rebuilds the ng component of every arc from a new prototype, with the same `create(arc)` call
/// `add_arc` makes. Every other component and each arc's value are untouched. Afterwards every
/// solve behaves exactly as on a graph built with @p neighborhoods.
///
/// **Not thread-safe with a solve on this graph**: call it only when no solve is running. Clones
/// taken earlier keep the old table; clones taken later carry the new one.
///
/// @tparam R     The container resource type the ng memory is stored in.
/// @tparam E     Element type of the sets.
/// @tparam Graph The `ResourceGraph` type, deduced.
/// @param graph           The graph.
/// @param component_index Which component of type @p R is the ng memory (0 for the first).
/// @param neighborhoods   The new table: the same keys, members among them (@ref check_ng_update).
/// @throws std::out_of_range     when there is no such component.
/// @throws std::invalid_argument when it is not ng-path, or the table breaks @ref check_ng_update.
template <typename R, typename E = typename R::ValueType, typename Graph>
void set_ng_neighborhoods(Graph& graph, size_t component_index,
                          std::map<size_t, std::set<E>> neighborhoods) {
    constexpr size_t kIndex = ng_detail::resource_types<Graph>::template index_of<R>;
    auto [factory, current] = ng_detail::ng_factory<R, E>(graph, component_index);
    check_ng_update(current->neighborhoods(), neighborhoods);

    factory->replace_extension_function(
        std::make_unique<NgPathExtensionFunction<R, E>>(std::move(neighborhoods)));
    // Every arc's copy was preprocessed with the old table; rebuild it from the new prototype.
    graph.for_each_arc([&](auto& arc) {
        arc.extender->template get_component<kIndex>(component_index)
            .replace_extension_function(factory->extension_function().create(arc));
    });
}

}  // namespace rcspp
