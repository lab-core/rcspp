// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <type_traits>

#include "rcspp/graph/graph.hpp"
#include "rcspp/validation/threshold_probe.hpp"

/// @brief Pairs of arcs to run a model's extension on, two steps in a row, and the comparison
///        of what the steps give.
///
/// Whether an extension adds a fixed amount per arc, and whether a backward search costs a path
/// as the forward search does, cannot be read off a declaration: the checks run the extension on
/// pairs of the model's own arcs (@c JoinCheck, @c BackwardExtensionCheck, and the test of the
/// completion bound, @c detail::find_share_shortfall).
namespace rcspp::arc_pair_probe {

/// @brief Calls @p fn(first, second) on pairs of arcs of the model, live or removed, so that
///        every arc is probed both first and second: each arc with one arc leaving its
///        destination, then each arc with one arc entering its origin.
///
/// An arc with no such neighbour is paired with itself: the probes only compose two steps, so the
/// two arcs need not be consecutive. Arcs without an extender are skipped. The model's own arcs
/// are a sample, not a proof: a step that misbehaves only from values that longer prefixes reach
/// still passes.
///
/// Costs two passes over the arcs.
///
/// @param graph   The graph.
/// @param removed The removed arcs, by the node they leave and enter
///                (@c threshold_probe::index_removed_arcs), for a node whose live arcs are gone.
/// @param fn      Called as `fn(first, second)`; returns whether to go on.
template <typename ResourceType, typename Callable>
void for_each_arc_pair(const Graph<ResourceType>& graph,
                       const threshold_probe::RemovedArcs<ResourceType>& removed, Callable&& fn) {
    bool going_on = true;
    threshold_probe::for_each_model_arc(graph, [&](const Arc<ResourceType>& arc) {
        if (!going_on || arc.extender == nullptr) {
            return;
        }
        const auto* next = threshold_probe::arc_to_probe(arc.destination->out_arcs,
                                                         removed.leaving,
                                                         arc.destination->id);
        going_on = fn(arc, next != nullptr ? *next : arc);
    });
    threshold_probe::for_each_model_arc(graph, [&](const Arc<ResourceType>& arc) {
        if (!going_on || arc.extender == nullptr) {
            return;
        }
        const auto* previous =
            threshold_probe::arc_to_probe(arc.origin->in_arcs, removed.entering, arc.origin->id);
        going_on = fn(previous != nullptr ? *previous : arc, arc);
    });
}

/// @brief Whether @p measured is @p expected: exactly for an integral type, to a relative 1e-9
///        for floating point, where an infinite value must match exactly.
///
/// @param measured The value a probe measured.
/// @param expected The value it should be.
/// @return Whether they match.
template <typename Scalar>
[[nodiscard]] bool is_close(Scalar measured, Scalar expected) {
    if (measured == expected) {
        return true;
    }
    if constexpr (std::is_floating_point_v<Scalar>) {
        if (!std::isfinite(measured) || !std::isfinite(expected)) {
            return false;  // an infinity is matched exactly, or not at all
        }
        constexpr Scalar kRelative = 1e-9;
        return std::abs(measured - expected) <=
               kRelative * std::max({Scalar{1}, std::abs(measured), std::abs(expected)});
    } else {
        return false;
    }
}

}  // namespace rcspp::arc_pair_probe
