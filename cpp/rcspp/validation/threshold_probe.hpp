// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "rcspp/graph/graph.hpp"
#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/functions/backward_kind.hpp"
#include "rcspp/resource/resource_traits.hpp"

/// @brief What the model checks read off the model by running its own functions, rather than
///        trusting its declarations: the clamps and starts a backward or forward step applies, and
///        the arcs (live or removed by preprocessing) to run them on.
///
/// The model checks (@c BackwardExtensionCheck, @c JoinCheck) and the bidirectional search's
/// half-way configuration share these probes.
namespace rcspp::threshold_probe {

/// @brief A value beyond every bound a model can state, which the arc's own step keeps
///        in range: above every bound going backward, below every bound going forward.
///
/// Floating point uses the infinities, then the finite extremes. An integral type uses
/// its extreme, moved by a negative consumption so that a translation cannot overflow,
/// then the value one step inside it.
///
/// @param consumption The arc's consumption.
/// @param backward    Whether the value goes backward.
/// @param second      Whether this is the second of the two values.
/// @return The value.
template <typename Scalar>
[[nodiscard]] Scalar beyond_bounds(Scalar consumption, bool backward, bool second) {
    using Limits = std::numeric_limits<Scalar>;
    if constexpr (std::is_floating_point_v<Scalar>) {
        if (backward) {
            return second ? Limits::max() : Limits::infinity();
        }
        return second ? Limits::lowest() : -Limits::infinity();
    } else {
        const bool negative = consumption < Scalar{0};
        if (backward) {
            const Scalar top = negative ? Limits::max() + consumption : Limits::max();
            return second ? top - 1 : top;
        }
        const Scalar bottom = negative ? Limits::lowest() - consumption : Limits::lowest();
        return second ? bottom + 1 : bottom;
    }
}

/// @brief A value just above @p value, beyond the tolerance the library's own comparisons
///        allow (@c value_leq adds the type's epsilon), so that a test of a ceiling at
///        @p value rejects it.
///
/// @param value The value.
/// @return A few units in the last place above @p value for floating point, the next
///         value for an integral type, and @p value itself at an integral type's maximum.
template <typename Scalar>
[[nodiscard]] Scalar just_above(Scalar value) {
    using Limits = std::numeric_limits<Scalar>;
    if constexpr (std::is_floating_point_v<Scalar>) {
        constexpr Scalar kSteps = 4;
        return value + (kSteps * Limits::epsilon() * std::max(Scalar{1}, std::abs(value)));
    } else {
        return value == Limits::max() ? value : static_cast<Scalar>(value + 1);
    }
}

/// @brief The lowest value a forward label can hold at a node: the forward clamp observed
///        there, or else 0, since values start at 0 and, once the negative-load scan has
///        ruled out a decrease, never fall.
///
/// @param forward_clamp The forward clamp observed at the node, if any.
/// @return The value.
[[nodiscard]] inline double lowest_forward_value(const std::optional<double>& forward_clamp) {
    return forward_clamp.value_or(0.0);
}

/// @brief The value @p extender starts a backward label with at its arc's destination,
///        or @c std::nullopt where it leaves the type default.
///
/// Two values beyond every bound are started in turn. A start sends both to one value; a
/// function that sets none keeps them apart, as @ref observe_clamps tells a clamp. Only
/// @c start_back is called, so a start is seen however the extension is written.
///
/// @param extender The component's extender, on an arc entering the sink.
/// @param scratch  A resource of the component's type, overwritten.
/// @return The start, or @c std::nullopt; always @c std::nullopt without a scalar value.
template <typename ComponentExtender, typename ComponentResource>
[[nodiscard]] std::optional<double> observe_start(const ComponentExtender& extender,
                                                  ComponentResource* scratch) {
    using Value = std::decay_t<decltype(scratch->get_value())>;
    if constexpr (is_numerical_resource_v<Value>) {
        using Scalar = std::decay_t<decltype(std::declval<Value>().get_value())>;
        scratch->set_value(beyond_bounds(Scalar{0}, /*backward=*/true, /*second=*/false));
        extender.start_back(scratch);
        const auto first = scratch->get_value().get_value();
        scratch->set_value(beyond_bounds(Scalar{0}, /*backward=*/true, /*second=*/true));
        extender.start_back(scratch);
        if (first == scratch->get_value().get_value()) {
            return static_cast<double>(first);
        }
    }
    return std::nullopt;
}

/// @brief Calls @p fn on every arc preprocessing removed.
///
/// Such an arc still belongs to the model, so the checks read it like a live one: a refusal does
/// not depend on what one solve's bounds happened to remove.
template <typename ResourceType, typename Fn>
void for_each_removed_arc(const Graph<ResourceType>& graph, Fn&& fn) {
    for (const size_t arc_id : graph.get_removed_arc_ids()) {
        if (const auto* arc = graph.get_removed_arc(arc_id); arc != nullptr) {
            fn(*arc);
        }
    }
}

/// @brief Calls @p fn on every arc of the model: the live ones, then the removed ones.
template <typename ResourceType, typename Fn>
void for_each_model_arc(const Graph<ResourceType>& graph, Fn&& fn) {
    graph.for_each_arc([&](const Arc<ResourceType>& arc) { fn(arc); });
    for_each_removed_arc(graph, fn);
}

/// @brief The removed arcs leaving and entering each node that has any, for the nodes
///        whose live arcs preprocessing took away.
template <typename ResourceType>
struct RemovedArcs {
        std::unordered_map<size_t, std::vector<const Arc<ResourceType>*>> leaving;
        std::unordered_map<size_t, std::vector<const Arc<ResourceType>*>> entering;
};

/// @brief Indexes the removed arcs by the node they leave and the node they enter.
///
/// @param graph The graph.
/// @return The index.
template <typename ResourceType>
[[nodiscard]] RemovedArcs<ResourceType> index_removed_arcs(const Graph<ResourceType>& graph) {
    RemovedArcs<ResourceType> removed;
    for_each_removed_arc(graph, [&](const Arc<ResourceType>& arc) {
        if (arc.extender != nullptr) {
            removed.leaving[arc.origin->id].push_back(&arc);
            removed.entering[arc.destination->id].push_back(&arc);
        }
    });
    return removed;
}

/// @brief An arc to read the model's declarations from: the first live arc with an extender, or
///        else the first such arc preprocessing removed.
///
/// One arc suffices: all extenders share a factory, hence a layout (not enforced). Reading a
/// removed arc when no live one is left keeps the verdict independent of what a bound removed.
///
/// @param graph The graph.
/// @return The arc, or @c nullptr when the model has none.
template <typename ResourceType>
[[nodiscard]] const Arc<ResourceType>* model_arc(const Graph<ResourceType>& graph) {
    for (size_t arc_id = 0; arc_id < graph.next_arc_id(); ++arc_id) {
        if (const auto* arc = graph.get_arc(arc_id); arc != nullptr && arc->extender != nullptr) {
            return arc;
        }
    }
    for (const size_t arc_id : graph.get_removed_arc_ids()) {
        if (const auto* arc = graph.get_removed_arc(arc_id);
            arc != nullptr && arc->extender != nullptr) {
            return arc;
        }
    }
    return nullptr;
}

/// @brief Each component's backward kind, as @ref model_arc's extender declares them.
///
/// @param graph The graph.
/// @return The kinds, in component order; empty when the model has no arc.
template <typename ResourceType>
[[nodiscard]] std::vector<BackwardKind> model_kinds(const Graph<ResourceType>& graph) {
    std::vector<BackwardKind> kinds;
    if (const auto* arc = model_arc(graph); arc != nullptr) {
        arc->extender->for_each_component(
            [&](const auto& component) { kinds.push_back(component.backward_kind()); });
    }
    return kinds;
}

/// @brief A node whose resource answers for all: any node that carries one.
///
/// @param graph The graph.
/// @return The node, or @c nullptr when no node carries a resource.
template <typename ResourceType>
[[nodiscard]] const Node<ResourceType>* node_with_resource(const Graph<ResourceType>& graph) {
    for (const size_t node_id : graph.get_node_ids()) {
        const auto* candidate = graph.get_node(node_id);
        if (candidate != nullptr && candidate->resource != nullptr) {
            return candidate;
        }
    }
    return nullptr;
}

/// @brief An arc to observe a node through: a live one, or else one that preprocessing
///        removed; the first of them that @p usable accepts.
///
/// @param live    The node's live arcs in the direction asked for.
/// @param removed The removed arcs, by the node they leave or enter.
/// @param node_id The node.
/// @param usable  Whether an arc can be probed.
/// @return The arc, or @c nullptr when the model has none that is usable.
template <typename ResourceType, typename Usable>
[[nodiscard]] const Arc<ResourceType>* arc_to_probe(
    const std::vector<Arc<ResourceType>*>& live,
    const std::unordered_map<size_t, std::vector<const Arc<ResourceType>*>>& removed,
    size_t node_id, const Usable& usable) {
    for (const auto* arc : live) {
        if (arc->extender != nullptr && usable(*arc)) {
            return arc;
        }
    }
    if (const auto it = removed.find(node_id); it != removed.end()) {
        for (const auto* arc : it->second) {
            if (usable(*arc)) {
                return arc;
            }
        }
    }
    return nullptr;
}

/// @brief An arc to observe a node through, whatever it consumes: a live one, or else one
///        that preprocessing removed.
template <typename ResourceType>
[[nodiscard]] const Arc<ResourceType>* arc_to_probe(
    const std::vector<Arc<ResourceType>*>& live,
    const std::unordered_map<size_t, std::vector<const Arc<ResourceType>*>>& removed,
    size_t node_id) {
    return arc_to_probe(live, removed, node_id, [](const Arc<ResourceType>& /*arc*/) {
        return true;
    });
}

/// @brief Whether every @c Threshold component of @p arc consumes a finite amount, so
///        that a probe through the arc can show the clamp at either end.
///
/// The probe pushes values beyond every bound through the arc. An infinite consumption,
/// the usual mark of a forbidden arc, turns them into NaN or an infinity of the other
/// sign (`inf - inf`, `max - inf`), so the two outputs differ and a clamp the extension
/// does apply would read as none.
///
/// @param arc   The arc; its extender must be set.
/// @param kinds Each component's backward kind.
/// @return Whether the arc can be probed for clamps.
template <typename ResourceType>
[[nodiscard]] bool consumes_finitely(const Arc<ResourceType>& arc,
                                     const std::vector<BackwardKind>& kinds) {
    bool finite = true;
    size_t index = 0;
    arc.extender->for_each_component([&](const auto& extender) {
        const size_t component_index = index++;
        using Value = std::decay_t<decltype(extender.get_value())>;
        if constexpr (is_numerical_resource_v<Value>) {
            using Scalar = std::decay_t<decltype(extender.get_value().get_value())>;
            if constexpr (std::is_floating_point_v<Scalar>) {
                if (component_index < kinds.size() &&
                    kinds[component_index] == BackwardKind::Threshold &&
                    !std::isfinite(extender.get_value().get_value())) {
                    finite = false;
                }
            }
        }
    });
    return finite;
}

/// @brief The clamps observed at one node, per component; reused from node to node.
struct NodeClamps {
        explicit NodeClamps(size_t count) : backward(count), forward(count), start(count) {}

        /// Whether an arc leaves the node, so that a backward step can arrive there.
        bool has_leaving = false;
        /// Whether an arc enters the node, so that a forward step can arrive there.
        bool has_entering = false;
        /// Whether the node is a sink an arc enters, so that a backward label starts there.
        bool has_start = false;
        /// The backward clamp, or @c std::nullopt where there is none.
        std::vector<std::optional<double>> backward;
        /// The forward clamp, or @c std::nullopt where there is none.
        std::vector<std::optional<double>> forward;
        /// The backward start, or @c std::nullopt where it is the type default.
        std::vector<std::optional<double>> start;
};

/// @brief Scratch resources for @ref observe_clamps: copied once per check from any node's
///        resource, then reused for every probe.
template <typename ResourceType>
struct ClampProbe {
        explicit ClampProbe(const Resource<ResourceType>& any)
            : first_input(any), second_input(any), first_output(any), second_output(any) {}

        Resource<ResourceType> first_input;
        Resource<ResourceType> second_input;
        Resource<ResourceType> first_output;
        Resource<ResourceType> second_output;
};

/// @brief What @p arc's extension does to values beyond every bound, per component: the
///        value a @c Threshold component clamps them to, or @c std::nullopt where its
///        output follows its input (and for every other component).
///
/// Two such values go through the arc, backward to observe the clamp at its origin, or
/// forward to observe the one at its destination. A clamp sends both to one value; a
/// function that does not clamp keeps them apart. Only @c extend and @c extend_back are
/// called, so a clamp is seen however the extension is written.
///
/// @param arc      The arc.
/// @param kinds    Each component's backward kind.
/// @param backward Whether to push the values backward.
/// @param probe    Reused inputs and outputs.
/// @param clamps   Receives one entry per component.
template <typename ResourceType>
void observe_clamps(const Arc<ResourceType>& arc, const std::vector<BackwardKind>& kinds,
                    bool backward, ClampProbe<ResourceType>* probe,
                    std::vector<std::optional<double>>* clamps) {
    const auto place = [&](Resource<ResourceType>* input, bool second) {
        size_t index = 0;
        input->for_each_component(*arc.extender, [&](auto& component, const auto& extender) {
            const size_t component_index = index++;
            using Value = std::decay_t<decltype(component.get_value())>;
            if constexpr (is_numerical_resource_v<Value>) {
                if (component_index < kinds.size() &&
                    kinds[component_index] == BackwardKind::Threshold) {
                    component.set_value(
                        beyond_bounds(extender->get_value().get_value(), backward, second));
                }
            }
        });
    };
    place(&probe->first_input, /*second=*/false);
    place(&probe->second_input, /*second=*/true);

    const auto push = [backward](auto& output, const auto& input, const auto& extender) {
        if (backward) {
            extender.extend_back(input, &output);
        } else {
            extender.extend(input, &output);
        }
    };
    probe->first_output.for_each_component(probe->first_input, *arc.extender, push);
    probe->second_output.for_each_component(probe->second_input, *arc.extender, push);

    size_t index = 0;
    probe->first_output.for_each_component(
        probe->second_output,
        [&](const auto& first, const auto& second) {
            const size_t component_index = index++;
            if (component_index >= clamps->size()) {
                return;
            }
            std::optional<double> clamp;
            using Value = std::decay_t<decltype(first.get_value())>;
            if constexpr (is_numerical_resource_v<Value>) {
                const auto value = first.get_value().get_value();
                if (kinds[component_index] == BackwardKind::Threshold &&
                    value == second->get_value().get_value()) {
                    clamp = static_cast<double>(value);
                }
            }
            (*clamps)[component_index] = clamp;
        });
}

/// @brief The start @p arc's extension gives a backward label at its destination, per
///        component: what a @c Threshold component sets (@ref observe_start), or
///        @c std::nullopt where it leaves the type default (and for every other component).
///
/// @param arc    An arc entering a sink.
/// @param kinds  Each component's backward kind.
/// @param probe  Reused scratch resources.
/// @param starts Receives one entry per component.
template <typename ResourceType>
void observe_starts(const Arc<ResourceType>& arc, const std::vector<BackwardKind>& kinds,
                    ClampProbe<ResourceType>* probe, std::vector<std::optional<double>>* starts) {
    size_t index = 0;
    probe->first_input.for_each_component(*arc.extender,
                                          [&](auto& component, const auto& extender) {
                                              const size_t component_index = index++;
                                              if (component_index >= starts->size()) {
                                                  return;
                                              }
                                              (*starts)[component_index] =
                                                  kinds[component_index] == BackwardKind::Threshold
                                                      ? observe_start(*extender, &component)
                                                      : std::nullopt;
                                          });
}

/// @brief Observes the clamps a backward step and a forward step arriving at @p node get,
///        and at a sink the start a backward label gets.
///
/// A backward step arrives over an arc leaving the node, a forward one over an arc
/// entering it. A node no arc leaves is never the arrival of a backward step, so it gets
/// no backward probe; likewise for the forward probe at a node no arc enters. A clamp is
/// probed only through an arc whose thresholds consume a finite amount
/// (@ref consumes_finitely), so a node whose arcs are all forbidden by an infinite
/// consumption is not probed. A backward label starts at a sink through an arc entering
/// it, as a backward search seeds a sink, whatever the arc consumes. A clamp or start
/// not probed reads as none.
///
/// @param node    The node.
/// @param removed The removed arcs, for a node whose live arcs are all gone.
/// @param kinds   Each component's backward kind.
/// @param probe   Reused scratch resources.
/// @param clamps  Receives the clamps.
template <typename ResourceType>
void observe_node_clamps(const Node<ResourceType>& node, const RemovedArcs<ResourceType>& removed,
                         const std::vector<BackwardKind>& kinds, ClampProbe<ResourceType>* probe,
                         NodeClamps* clamps) {
    const auto finite = [&kinds](const Arc<ResourceType>& arc) {
        return consumes_finitely(arc, kinds);
    };
    const auto* leaving = arc_to_probe(node.out_arcs, removed.leaving, node.id, finite);
    const auto* entering = arc_to_probe(node.in_arcs, removed.entering, node.id, finite);
    const auto* starting =
        node.sink ? arc_to_probe(node.in_arcs, removed.entering, node.id) : nullptr;
    clamps->has_leaving = leaving != nullptr;
    clamps->has_entering = entering != nullptr;
    clamps->has_start = starting != nullptr;
    if (leaving != nullptr) {
        observe_clamps(*leaving, kinds, /*backward=*/true, probe, &clamps->backward);
    } else {
        std::ranges::fill(clamps->backward, std::nullopt);
    }
    if (entering != nullptr) {
        observe_clamps(*entering, kinds, /*backward=*/false, probe, &clamps->forward);
    } else {
        std::ranges::fill(clamps->forward, std::nullopt);
    }
    if (clamps->has_start) {
        observe_starts(*starting, kinds, probe, &clamps->start);
    } else {
        std::ranges::fill(clamps->start, std::nullopt);
    }
}

}  // namespace rcspp::threshold_probe
