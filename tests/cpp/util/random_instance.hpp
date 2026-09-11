// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Phase 13: the randomised instance generator behind the equivalence suite.
//
// Two rules shape this file.
//
// **Every build is a fresh graph.** `solve()` preprocesses the graph in place -- the feasibility
// preprocessor removes arcs -- so the reference run and the candidate run cannot share one. Every
// function here takes a config and returns a newly built graph, deterministic in the config's seed,
// so "the same instance" means "the same config" rather than "the same object".
//
// **A failing case must be replayable.** `describe()` prints every field including the seed, and
// every assertion in the suite attaches it. A property test whose failures cannot be reproduced is
// a property test that gets disabled.

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

namespace test_util {

/// @brief The knobs the equivalence suite sweeps.
///
/// Kept as plain data so a failing case is one printable line, and so the sweep can enumerate
/// combinations rather than hand-write them.
struct InstanceConfig {
        /// @brief Node count. Small enough that the reference algorithm finishes quickly.
        size_t num_nodes = 10;

        /// @brief Probability that an arc `i -> j` (`i < j`) beyond the spine exists.
        ///
        /// Arcs only ever go forwards in node order, so the graph is a DAG: the reference search
        /// terminates on every instance and the brute-force enumerator has something finite to
        /// enumerate. Cycles are covered by the hand-built instances elsewhere in the suite.
        double density = 0.5;

        /// @brief Adds a time-window component -- the clock a bidirectional solve can use.
        bool with_time_window = false;

        /// @brief 0 leaves no slack beyond the earliest arrival; larger values loosen the windows.
        double window_slack = 1.0;

        /// @brief Adds a capacity component, as a budget (`Threshold`), not an addition.
        bool with_capacity = false;

        /// @brief Whether the capacity actually binds, or is set far above any path's load.
        bool capacity_binds = false;

        /// @brief Lets arc costs go negative, as reduced costs do during pricing.
        ///
        /// This is what makes the completion bound earn its keep: a half costing 30 can complete
        /// to -20, so pruning a half against the incumbent would discard the best columns.
        bool mixed_sign_costs = false;

        /// @brief One sink, or two with different windows -- the latter exercises per-node seeding.
        size_t num_sinks = 1;

        /// @brief Adds a constant to each arc's `arc.cost`, leaving the cost *component* alone.
        ///
        /// `arc.cost` is the ORIGINAL arc weight and the cost component is what the labels
        /// accumulate; `ResourceGraph::update_reduced_costs` rewrites the second and leaves the
        /// first, so in a real pricing model they are different numbers. With this at its default
        /// of 0 the generator makes them equal, which is convenient and which hid a bound that
        /// relaxed on the wrong one (review finding D1).
        ///
        /// A *positive* offset is the dangerous direction: it makes any bound summed from
        /// `arc.cost` over-estimate the remaining labeling cost by `offset x path length`, which is
        /// precisely the inadmissibility that loses the optimum. Nothing in the model's semantics
        /// changes -- `arc.cost` is read only when building a column's original cost.
        double arc_cost_offset = 0.0;

        /// @brief The generator's seed. Printed on every failure.
        std::uint32_t seed = 0;
};

/// @brief One instance, with the facts the suite needs to configure a bidirectional solve.
struct GeneratedInstance {
        std::unique_ptr<rcspp::ResourceGraph<rcspp::RealResource>> graph;

        /// @brief Component index of the clock, within the `RealResource` slot.
        ///
        /// The time window when there is one, else the capacity, else the cost -- and the cost is
        /// not a clock, so that last case is the one where the algorithm must notice and switch
        /// the half-way bound off rather than trust it.
        size_t clock_index = 0;

        /// @brief `R`, the clock's finite maximum; `H` is derived from it as `R / 2`.
        double clock_upper_bound = 0.0;

        /// @brief Whether the clock is a resource the half-way bound can legitimately read.
        bool clock_is_usable = false;
};

/// @brief Renders a config as one line, for a failure message.
inline std::string describe(const InstanceConfig& config) {
    std::string text =
        "seed=" + std::to_string(config.seed) + " nodes=" + std::to_string(config.num_nodes) +
        " density=" + std::to_string(config.density) + " sinks=" + std::to_string(config.num_sinks);
    text += config.with_time_window
                ? " time_window=yes slack=" + std::to_string(config.window_slack)
                : " time_window=no";
    text += config.with_capacity ? (config.capacity_binds ? " capacity=binding" : " capacity=loose")
                                 : " capacity=no";
    text += config.mixed_sign_costs ? " costs=mixed" : " costs=positive";
    // Only when set, so every existing failure message stays byte-identical.
    if (config.arc_cost_offset != 0.0) {
        text += " arc_cost_offset=" + std::to_string(config.arc_cost_offset);
    }
    return text;
}

/// @brief The generator's raw draw, before any graph is built.
///
/// Separated out so the same numbers can be replayed into a graph, into the brute-force
/// enumerator, and into a description -- all three must agree on what the instance *is*.
struct InstanceDraw {
        struct Arc {
                size_t origin = 0;
                size_t destination = 0;
                double cost = 0.0;
                double time = 0.0;
                double load = 0.0;
        };

        std::vector<Arc> arcs;
        std::vector<size_t> sinks;
        std::map<size_t, std::pair<double, double>> windows;
        double capacity = 0.0;
        double horizon = 0.0;
};

/// @brief Draws one instance from @p config.
///
/// The spine `0 -> 1 -> ... -> n-1` is always present, so there is always at least one
/// source-to-sink path and "no solutions" always means the resource constraints cut it, never that
/// the graph fell apart. Every other arc is drawn independently.
inline InstanceDraw draw_instance(const InstanceConfig& config) {
    std::mt19937 rng(config.seed);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    std::uniform_real_distribution<double> positive_cost(1.0, 10.0);
    std::uniform_real_distribution<double> mixed_cost(-6.0, 10.0);
    std::uniform_real_distribution<double> time_draw(1.0, 5.0);
    std::uniform_real_distribution<double> load_draw(1.0, 4.0);

    InstanceDraw draw;

    auto next_cost = [&]() {
        return config.mixed_sign_costs ? mixed_cost(rng) : positive_cost(rng);
    };

    for (size_t i = 0; i + 1 < config.num_nodes; ++i) {
        draw.arcs.push_back({i, i + 1, next_cost(), time_draw(rng), load_draw(rng)});
    }
    for (size_t i = 0; i < config.num_nodes; ++i) {
        for (size_t j = i + 2; j < config.num_nodes; ++j) {
            if (unit(rng) < config.density) {
                draw.arcs.push_back({i, j, next_cost(), time_draw(rng), load_draw(rng)});
            }
        }
    }

    draw.sinks.push_back(config.num_nodes - 1);
    if (config.num_sinks > 1 && config.num_nodes > 2) {
        draw.sinks.push_back(config.num_nodes - 2);
    }

    // Earliest arrival along the spine, which every window has to admit or the instance is
    // trivially infeasible and tests nothing.
    std::vector<double> earliest(config.num_nodes, 0.0);
    for (const auto& arc : draw.arcs) {
        if (arc.destination == arc.origin + 1) {
            earliest[arc.destination] = earliest[arc.origin] + arc.time;
        }
    }
    draw.horizon = earliest.back() * (1.0 + config.window_slack) + 1.0;
    for (size_t i = 0; i < config.num_nodes; ++i) {
        draw.windows[i] = {0.0, earliest[i] * (1.0 + config.window_slack) + 1.0};
    }

    double spine_load = 0.0;
    for (const auto& arc : draw.arcs) {
        if (arc.destination == arc.origin + 1) {
            spine_load += arc.load;
        }
    }
    draw.capacity = config.capacity_binds ? spine_load * 0.75 : spine_load * 10.0;

    return draw;
}

/// @brief Builds a graph for @p config, and reports how the clock should be configured.
///
/// Components, in order: cost (slot 0), time window (slot 1, optional), capacity (next slot,
/// optional). The capacity is a `BudgetExtensionFunction`, never an addition: paired with
/// `MinMaxFeasibilityFunction` an addition is the incoherent combination the algorithm refuses at
/// setup, so generating it here would test the refusal rather than the search.
inline GeneratedInstance build_instance(const InstanceConfig& config) {
    using namespace rcspp;  // NOLINT(google-build-using-namespace)

    const InstanceDraw draw = draw_instance(config);

    GeneratedInstance built;
    built.graph = std::make_unique<ResourceGraph<RealResource>>();

    built.graph->add_resource<RealResource>(
        std::make_unique<AdditionExtensionFunction<RealResource>>(),
        std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<ValueCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());

    size_t next_slot = 1;
    if (config.with_time_window) {
        built.graph->add_resource<RealResource>(
            std::make_unique<TimeWindowExtensionFunction<RealResource>>(draw.windows),
            std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(draw.windows),
            std::make_unique<TrivialCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
        built.clock_index = next_slot;
        built.clock_upper_bound = draw.horizon;
        built.clock_is_usable = true;
        ++next_slot;
    }
    if (config.with_capacity) {
        built.graph->add_resource<RealResource>(
            std::make_unique<BudgetExtensionFunction<RealResource>>(),
            std::make_unique<MinMaxFeasibilityFunction<RealResource>>(
                0.0,
                draw.capacity,
                /*merge_by_increasing_value=*/true),
            std::make_unique<TrivialCostFunction<RealResource>>(),
            std::make_unique<ValueDominanceFunction<RealResource>>());
        if (!built.clock_is_usable) {
            built.clock_index = next_slot;
            built.clock_upper_bound = draw.capacity;
            built.clock_is_usable = true;
        }
        ++next_slot;
    }
    // No clock at all: the cost slot is the only candidate, and cost is not a clock. The bound has
    // to switch itself off here, which is a case worth generating rather than avoiding.
    if (!built.clock_is_usable) {
        built.clock_index = 0;
        built.clock_upper_bound = 0.0;
    }

    const std::set<size_t> sinks(draw.sinks.begin(), draw.sinks.end());
    for (size_t node_id = 0; node_id < config.num_nodes; ++node_id) {
        built.graph->add_node(node_id, node_id == 0, sinks.contains(node_id));
    }

    for (const auto& arc : draw.arcs) {
        // The tuple carries the cost COMPONENT, which the labels accumulate; the last argument is
        // `arc.cost`, the ORIGINAL weight, which only column construction reads. Equal by default;
        // `arc_cost_offset` separates them, which is the shape update_reduced_costs produces.
        const double original_weight = arc.cost + config.arc_cost_offset;
        if (config.with_time_window && config.with_capacity) {
            built.graph->add_arc<RealResource, RealResource, RealResource>(
                {arc.cost, arc.time, arc.load},
                arc.origin,
                arc.destination,
                original_weight);
        } else if (config.with_time_window) {
            built.graph->add_arc<RealResource, RealResource>({arc.cost, arc.time},
                                                             arc.origin,
                                                             arc.destination,
                                                             original_weight);
        } else if (config.with_capacity) {
            built.graph->add_arc<RealResource, RealResource>({arc.cost, arc.load},
                                                             arc.origin,
                                                             arc.destination,
                                                             original_weight);
        } else {
            built.graph->add_arc<RealResource>(std::make_tuple(arc.cost),
                                               arc.origin,
                                               arc.destination,
                                               original_weight);
        }
    }

    return built;
}

/// @brief The true optimum, by enumerating every simple path from the source to a sink.
///
/// An independent oracle: it shares no code with either algorithm, so an error common to both --
/// a dominance rule that discards something it should not -- still shows up here. Exponential, so
/// the suite only calls it on the small, sparse end of the sweep.
///
/// @param config The instance's config; the same draw is replayed.
/// @return The optimal cost, or infinity when no feasible path exists.
inline double brute_force_optimum(const InstanceConfig& config) {
    const InstanceDraw draw = draw_instance(config);
    const std::set<size_t> sinks(draw.sinks.begin(), draw.sinks.end());

    std::vector<std::vector<const InstanceDraw::Arc*>> out_arcs(config.num_nodes);
    for (const auto& arc : draw.arcs) {
        out_arcs[arc.origin].push_back(&arc);
    }

    double best = std::numeric_limits<double>::infinity();

    // Iterative depth-first walk over (node, cost, time, load). Arcs go forwards in node order, so
    // no visited set is needed: a path cannot revisit a node.
    struct State {
            size_t node;
            double cost;
            double time;
            double load;
    };
    std::vector<State> stack{{0, 0.0, 0.0, 0.0}};
    while (!stack.empty()) {
        const State state = stack.back();
        stack.pop_back();

        if (sinks.contains(state.node)) {
            best = std::min(best, state.cost);
            // Not `continue`: a sink may still have outgoing arcs to a further sink.
        }
        for (const auto* arc : out_arcs[state.node]) {
            // The time window extension waits for the opening time; every window here opens at 0,
            // so arrival is plain accumulation.
            const double time = state.time + arc->time;
            const double load = state.load + arc->load;
            if (config.with_time_window && time > draw.windows.at(arc->destination).second) {
                continue;
            }
            if (config.with_capacity && load > draw.capacity) {
                continue;
            }
            stack.push_back({arc->destination, state.cost + arc->cost, time, load});
        }
    }

    return best;
}

}  // namespace test_util
