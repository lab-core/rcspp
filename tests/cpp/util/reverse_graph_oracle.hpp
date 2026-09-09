// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <memory>
#include <tuple>
#include <vector>

#include "rcspp/rcspp.hpp"

namespace test_util {

/// @brief One arc of an additive test instance.
struct OracleArc {
        size_t origin;
        size_t destination;
        double cost;
        double load;
};

/// @brief A small instance description, from which the forward graph and its reversal are built.
///
/// Built from a description rather than by cloning a graph and swapping endpoints: an arc's
/// resource consumption is held inside its `Extender` and is not readable back out, so re-adding
/// arcs from the description is both simpler and less likely to quietly lose a component.
struct AdditiveInstance {
        size_t num_nodes = 0;
        std::vector<size_t> sources;
        std::vector<size_t> sinks;
        std::vector<OracleArc> arcs;

        /// @brief Upper bound on the accumulated load; <= 0 builds a cost-only model.
        double capacity = 0.0;
};

/// @brief Builds a `ResourceGraph` for @p instance, optionally with every arc reversed.
///
/// **VALID ONLY for additive models.** A reversed arc still applies the *forward* extension
/// function, so a time-window or ng-path model run through this would produce a confident, wrong
/// answer. That limitation is exactly what makes it an independent check rather than a second
/// implementation of the backward search: it shares no code with `BackwardDirection`.
///
/// Every resource this builds is therefore `AdditionExtensionFunction`, whose `backward_kind()` is
/// `Accumulate` -- forward and backward extension coincide. `OracleBuildsOnlyAdditiveResources`
/// pins that, so a future edit that slips a threshold resource in here fails loudly.
///
/// The load component, when present, uses `MinMaxFeasibilityFunction` with
/// `merge_by_increasing_value = false` on purpose: that makes `back_seed_value()` return the
/// *minimum* (0), so a backward label accumulates load from the sink exactly as a forward one
/// accumulates it from the source. With the default `true` it would seed at the capacity and then
/// *add*, which is the threshold reading -- correct for a resource whose extension subtracts, and
/// wrong for one that adds.
///
/// @param instance The instance description.
/// @param reversed When true, swap every arc's endpoints and exchange the source/sink flags.
/// @return An owning pointer to the built graph.
inline std::unique_ptr<rcspp::ResourceGraph<rcspp::RealResource>> build_additive_graph(
    const AdditiveInstance& instance, bool reversed) {
    using rcspp::RealResource;

    auto graph = std::make_unique<rcspp::ResourceGraph<RealResource>>();

    // Component 0: cost. Accumulate, unbounded.
    graph->add_resource<RealResource>(
        std::make_unique<rcspp::AdditionExtensionFunction<RealResource>>(),
        std::make_unique<rcspp::TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<rcspp::ValueCostFunction<RealResource>>(),
        std::make_unique<rcspp::ValueDominanceFunction<RealResource>>());

    const bool with_load = instance.capacity > 0.0;
    if (with_load) {
        // Component 1: load. Accumulate, bounded by [0, capacity]. See the note above on why the
        // merge-direction flag is false.
        graph->add_resource<RealResource>(
            std::make_unique<rcspp::AdditionExtensionFunction<RealResource>>(),
            std::make_unique<rcspp::MinMaxFeasibilityFunction<RealResource>>(
                0.0,
                instance.capacity,
                /*merge_by_increasing_value=*/false),
            std::make_unique<rcspp::TrivialCostFunction<RealResource>>(),
            std::make_unique<rcspp::ValueDominanceFunction<RealResource>>());
    }

    for (size_t node_id = 0; node_id < instance.num_nodes; ++node_id) {
        const bool is_source =
            std::ranges::find(instance.sources, node_id) != instance.sources.end();
        const bool is_sink = std::ranges::find(instance.sinks, node_id) != instance.sinks.end();
        // Reversing exchanges the roles: a reversed graph starts where the original ended.
        graph->add_node(node_id, reversed ? is_sink : is_source, reversed ? is_source : is_sink);
    }

    for (const auto& arc : instance.arcs) {
        const size_t origin = reversed ? arc.destination : arc.origin;
        const size_t destination = reversed ? arc.origin : arc.destination;
        if (with_load) {
            graph->add_arc<RealResource, RealResource>({arc.cost, arc.load},
                                                       origin,
                                                       destination,
                                                       arc.cost);
        } else {
            graph->add_arc<RealResource>(std::make_tuple(arc.cost), origin, destination, arc.cost);
        }
    }

    return graph;
}

}  // namespace test_util
