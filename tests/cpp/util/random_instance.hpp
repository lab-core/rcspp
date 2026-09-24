// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Randomised instance generator for the equivalence suite.
//
// `solve()` modifies the graph in place, so each run needs a fresh graph: every builder here is
// deterministic in the config's seed. `describe()` prints the whole config so failures replay.

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
struct InstanceConfig {
        /// @brief Node count.
        size_t num_nodes = 10;

        /// @brief Probability that an arc `i -> j` (`i < j`) beyond the spine exists.
        ///
        /// Arcs only go forwards in node order, so the graph is a DAG.
        double density = 0.5;

        /// @brief Adds a time-window component, usable as the bidirectional clock.
        bool with_time_window = false;

        /// @brief 0 leaves no slack beyond the earliest arrival; larger values loosen the windows.
        double window_slack = 1.0;

        /// @brief Adds a capacity component, as a budget (`Threshold`), not an addition.
        bool with_capacity = false;

        /// @brief Whether the capacity actually binds, or is set far above any path's load.
        bool capacity_binds = false;

        /// @brief Lets arc costs go negative, as reduced costs do during pricing.
        bool mixed_sign_costs = false;

        /// @brief One sink, or two with different windows (exercises per-node seeding).
        size_t num_sinks = 1;

        /// @brief Multiplies a shortcut `i -> j`'s cost, time and load by `j - i`.
        ///
        /// Off, a shortcut draws what a spine arc draws, so the optimum takes a few shortcuts and
        /// reaches the sink early on the clock, rarely crossing `H` at an interior node. On, a
        /// shortcut costs about what the spine it skips costs, so optima use more of the clock and
        /// the join has work to do.
        bool span_scaled_shortcuts = false;

        /// @brief Adds a constant to each arc's `arc.cost`, leaving the cost *component* alone.
        ///
        /// Makes the original weight differ from the labeled cost, as after
        /// `update_reduced_costs`. A positive offset catches bounds wrongly built from `arc.cost`.
        double arc_cost_offset = 0.0;

        /// @brief The generator's seed. Printed on every failure.
        std::uint32_t seed = 0;
};

/// @brief One instance, with the facts the suite needs to configure a bidirectional solve.
struct GeneratedInstance {
        std::unique_ptr<rcspp::ResourceGraph<rcspp::RealResource>> graph;

        /// @brief Component index of the clock: the time window, else the capacity, else the cost
        /// (not a real clock, so the half-way bound must switch itself off).
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
    // Only printed when set.
    if (config.span_scaled_shortcuts) {
        text += " shortcuts=span-scaled";
    }
    if (config.arc_cost_offset != 0.0) {
        text += " arc_cost_offset=" + std::to_string(config.arc_cost_offset);
    }
    return text;
}

/// @brief The generator's raw draw, shared by the graph builder and the brute-force oracle.
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
/// The spine `0 -> 1 -> ... -> n-1` is always present, so infeasibility can only come from the
/// resource constraints. Every other arc is drawn independently.
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
                // Drawn before scaling, so the knob leaves the random sequence alone.
                InstanceDraw::Arc arc{i, j, next_cost(), time_draw(rng), load_draw(rng)};
                if (config.span_scaled_shortcuts) {
                    const auto span = static_cast<double>(j - i);
                    arc.cost *= span;
                    arc.time *= span;
                    arc.load *= span;
                }
                draw.arcs.push_back(arc);
            }
        }
    }

    draw.sinks.push_back(config.num_nodes - 1);
    if (config.num_sinks > 1 && config.num_nodes > 2) {
        draw.sinks.push_back(config.num_nodes - 2);
    }

    // Windows always admit the earliest arrival along the spine.
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
/// Components, in order: cost, optional time window, optional capacity. The capacity is a budget
/// so that, without a time window, it can be the clock: an addition is not a threshold.
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
    // No clock at all: the half-way bound must switch itself off.
    if (!built.clock_is_usable) {
        built.clock_index = 0;
        built.clock_upper_bound = 0.0;
    }

    const std::set<size_t> sinks(draw.sinks.begin(), draw.sinks.end());
    for (size_t node_id = 0; node_id < config.num_nodes; ++node_id) {
        built.graph->add_node(node_id, node_id == 0, sinks.contains(node_id));
    }

    for (const auto& arc : draw.arcs) {
        // The tuple holds the labeled cost component; the last argument is the original weight.
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
/// Shares no code with the algorithms, so it catches errors common to both. Exponential: only use
/// on small instances. By default a path ends at the first sink it reaches, as in the library.
///
/// @param config               The instance's config; the same draw is replayed.
/// @param allow_interior_sinks Whether a walk may pass through a sink and continue (not the
///                             library's model; kept for comparison).
/// @return The optimal cost, or infinity when no feasible path exists.
inline double brute_force_optimum(const InstanceConfig& config, bool allow_interior_sinks = false) {
    const InstanceDraw draw = draw_instance(config);
    const std::set<size_t> sinks(draw.sinks.begin(), draw.sinks.end());

    std::vector<std::vector<const InstanceDraw::Arc*>> out_arcs(config.num_nodes);
    for (const auto& arc : draw.arcs) {
        out_arcs[arc.origin].push_back(&arc);
    }

    double best = std::numeric_limits<double>::infinity();

    // Iterative DFS. The graph is a DAG, so no visited set is needed.
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
            if (!allow_interior_sinks) {
                continue;  // a path ends at the first sink it reaches
            }
            // else: fall through -- a sink may still have outgoing arcs to a further sink
        }
        for (const auto* arc : out_arcs[state.node]) {
            // Every window opens at 0, so there is never any waiting.
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
