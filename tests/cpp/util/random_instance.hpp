// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Randomised instance generator for the equivalence suite.
//
// `solve()` modifies the graph in place, so each run needs a fresh graph: every builder here is
// deterministic in the config's seed. `describe()` prints the whole config so failures replay.

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <stdexcept>
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

        /// @brief Adds an ng-path component (`NgPathExtensionFunction` +
        ///        `IntersectionFeasibilityFunction`); read only by `build_ng_instance`.
        ///
        /// On an acyclic instance it can never reject anything; set @c back_arc_density to test
        /// ng semantics. When false the component is still registered, with empty forbidden sets,
        /// so the label width is the same either way.
        bool with_ng_path = false;

        /// @brief Probability of an arc `j -> i` with `j > i`, which introduces cycles.
        ///
        /// 0 keeps the graph a DAG, which `brute_force_optimum` requires. Above 0 requires
        /// @c with_time_window (the horizon bounds path length) and should be paired with
        /// @c mixed_sign_costs, or no shortest path revisits a node and ng never binds.
        double back_arc_density = 0.0;

        /// @brief Share of nodes whose ng forbidden entry is dropped, so a path may revisit them
        ///        although the neighbourhoods still mention them. Read by `build_ng_instance` and
        ///        `ng_cyclic_optimum`.
        ///
        /// The subset comes from its own generator, so the instance draw is unchanged.
        double ng_revisitable_share = 0.0;
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
    text += config.with_ng_path ? " ng=yes" : " ng=no";
    if (config.back_arc_density > 0.0) {
        text += " back_arcs=" + std::to_string(config.back_arc_density);
    }
    if (config.ng_revisitable_share > 0.0) {
        text += " ng_revisitable=" + std::to_string(config.ng_revisitable_share);
    }
    // Printed only when set.
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
    // Cyclic instances need a time window: nothing else bounds path length. Mixed-sign costs
    // with back arcs are fine, since the horizon keeps the optimum finite.
    if (config.back_arc_density > 0.0 && !config.with_time_window) {
        throw std::logic_error(
            "back_arc_density > 0 without with_time_window: on a cyclic graph nothing bounds path "
            "length, so the forward search does not terminate. The window horizon is what makes a "
            "cyclic instance finite");
    }

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

    // Back arcs are drawn last so that, for a given seed, the forward arcs are the same whatever
    // `back_arc_density` is.
    if (config.back_arc_density > 0.0) {
        for (size_t j = 0; j < config.num_nodes; ++j) {
            for (size_t i = 0; i < j; ++i) {
                if (unit(rng) < config.back_arc_density) {
                    draw.arcs.push_back({j, i, next_cost(), time_draw(rng), load_draw(rng)});
                }
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
    // On a cyclic instance the windows are widened so a path has room for at least one detour;
    // otherwise no path could revisit a node and ng could not bind.
    const double cyclic_slack = config.back_arc_density > 0.0 ? 2.5 : 1.0;
    draw.horizon = earliest.back() * (1.0 + config.window_slack) * cyclic_slack + 1.0;
    for (size_t i = 0; i < config.num_nodes; ++i) {
        draw.windows[i] = {0.0, earliest[i] * (1.0 + config.window_slack) * cyclic_slack + 1.0};
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

/// @brief The ng variant of @c GeneratedInstance: a two-slot pack with a container component.
struct NgGeneratedInstance {
        std::unique_ptr<rcspp::ResourceGraph<rcspp::RealResource, rcspp::SizeTBitsetResource>>
            graph;

        /// @brief Component index of the clock, *within the RealResource slot*.
        ///
        /// The ng component lives in the other type slot, so this matches @c GeneratedInstance.
        size_t clock_index = 0;

        /// @brief `R`, the clock's finite maximum; `H` is derived from it as `R / 2`.
        double clock_upper_bound = 0.0;

        /// @brief Whether the clock is a resource the half-way bound can legitimately read.
        bool clock_is_usable = false;
};

/// @brief The ng neighborhood of each node: a window of node ids centred on it.
///
/// A window rather than the whole node set, so the relaxation is ng-path rather than plain
/// elementarity. The width decides whether ng actually binds.
/// @brief The nodes whose ng forbidden entry @p config drops; see
///        @c InstanceConfig::ng_revisitable_share. Never the source.
inline std::set<size_t> ng_revisitable_nodes(const InstanceConfig& config) {
    std::set<size_t> nodes;
    if (config.ng_revisitable_share <= 0.0) {
        return nodes;
    }
    constexpr std::uint32_t kSalt = 7919U;  // a generator of its own, so the draw is unchanged
    std::mt19937 rng(config.seed + kSalt);
    std::bernoulli_distribution revisitable(config.ng_revisitable_share);
    for (size_t node = 1; node < config.num_nodes; ++node) {
        if (revisitable(rng)) {
            nodes.insert(node);
        }
    }
    return nodes;
}

inline std::map<size_t, std::set<size_t>> ng_neighborhoods(size_t num_nodes, size_t width = 3) {
    std::map<size_t, std::set<size_t>> neighborhoods;
    for (size_t node = 0; node < num_nodes; ++node) {
        std::set<size_t> neighborhood;
        const size_t low = node > width ? node - width : 0;
        const size_t high = std::min(num_nodes - 1, node + width);
        for (size_t other = low; other <= high; ++other) {
            neighborhood.insert(other);
        }
        neighborhoods[node] = std::move(neighborhood);
    }
    return neighborhoods;
}

/// @brief Builds the ng model from the SAME draw the single-type builder uses.
///
/// Sharing @c draw_instance means both builders see identical arcs, windows and capacities for a
/// given seed. The ng arcs carry an empty set: @c NgPathExtensionFunction takes the node it adds
/// from the arc's endpoints and ignores the arc value.
///
/// @param config The instance to build. @c with_ng_path decides whether the ng component
///               constrains anything; it is registered either way.
/// @return The graph and the clock configuration.
inline NgGeneratedInstance build_ng_instance(const InstanceConfig& config) {
    using namespace rcspp;  // NOLINT(google-build-using-namespace)

    const InstanceDraw draw = draw_instance(config);

    NgGeneratedInstance built;
    built.graph = std::make_unique<ResourceGraph<RealResource, SizeTBitsetResource>>();

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
    if (!built.clock_is_usable) {
        built.clock_index = 0;
        built.clock_upper_bound = 0.0;
    }

    // The ng component, always registered. `forbidden_by_node[v] = {v}` makes arriving at a
    // remembered node infeasible; with the flag off the sets are empty and the component is inert.
    // A revisitable node has no entry, though the neighbourhoods still mention it.
    std::map<size_t, std::set<size_t>> forbidden_by_node;
    if (config.with_ng_path) {
        const std::set<size_t> revisitable = ng_revisitable_nodes(config);
        for (size_t node = 0; node < config.num_nodes; ++node) {
            if (!revisitable.contains(node)) {
                forbidden_by_node[node] = {node};
            }
        }
    }
    built.graph->add_resource<SizeTBitsetResource>(
        std::make_unique<NgPathExtensionFunction<SizeTBitsetResource, size_t>>(
            ng_neighborhoods(config.num_nodes)),
        std::make_unique<IntersectionFeasibilityFunction<SizeTBitsetResource, size_t>>(
            std::move(forbidden_by_node),
            /*forbidden=*/true),
        std::make_unique<TrivialCostFunction<SizeTBitsetResource>>(),
        std::make_unique<InclusionDominanceFunction<SizeTBitsetResource>>());

    const std::set<size_t> sinks(draw.sinks.begin(), draw.sinks.end());
    for (size_t node_id = 0; node_id < config.num_nodes; ++node_id) {
        built.graph->add_node(node_id, node_id == 0, sinks.contains(node_id));
    }

    for (const auto& arc : draw.arcs) {
        // As in build_instance: the tuple carries the cost component, the last argument the
        // original weight.
        const double original_weight = arc.cost + config.arc_cost_offset;
        const std::set<size_t> no_arc_set;  // the ng extension ignores the arc value
        if (config.with_time_window && config.with_capacity) {
            built.graph->add_arc<RealResource, RealResource, RealResource, SizeTBitsetResource>(
                std::make_tuple(std::make_tuple(arc.cost),
                                std::make_tuple(arc.time),
                                std::make_tuple(arc.load),
                                std::make_tuple(no_arc_set)),
                arc.origin,
                arc.destination,
                original_weight);
        } else if (config.with_time_window) {
            built.graph->add_arc<RealResource, RealResource, SizeTBitsetResource>(
                std::make_tuple(std::make_tuple(arc.cost),
                                std::make_tuple(arc.time),
                                std::make_tuple(no_arc_set)),
                arc.origin,
                arc.destination,
                original_weight);
        } else if (config.with_capacity) {
            built.graph->add_arc<RealResource, RealResource, SizeTBitsetResource>(
                std::make_tuple(std::make_tuple(arc.cost),
                                std::make_tuple(arc.load),
                                std::make_tuple(no_arc_set)),
                arc.origin,
                arc.destination,
                original_weight);
        } else {
            built.graph->add_arc<RealResource, SizeTBitsetResource>(
                std::make_tuple(std::make_tuple(arc.cost), std::make_tuple(no_arc_set)),
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
    // The walk has no visited set and relies on arcs going forwards, so refuse cyclic instances.
    if (config.back_arc_density > 0.0) {
        throw std::logic_error(
            "brute_force_optimum enumerates simple paths by walking forwards in node order and so "
            "requires an acyclic instance; back_arc_density must be 0");
    }

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

/// @brief The true optimum of the **cyclic ng** model, by enumerating walks.
///
/// @c brute_force_optimum cannot handle cycles. This one allows revisits and tracks the ng memory
/// explicitly as `(memory & neighborhood[node_left]) | {node_left}`, independently of either
/// search. The time window keeps the walk count finite; @p state_budget makes it throw rather
/// than return a truncated answer.
///
/// @param config              The instance. Must set @c with_time_window.
/// @param state_budget        Maximum walk states to expand before giving up.
/// @param allow_interior_sinks Whether a walk may pass through a sink and continue. The default,
///                            @c false, ends a path at the first sink, as the library does;
///                            @c true keeps the older, more permissive model for comparison.
/// @return The optimal cost, or infinity when no feasible walk reaches a sink.
/// @throws std::logic_error If the budget is exhausted, or the config has no time window.
inline double ng_cyclic_optimum(const InstanceConfig& config, long long state_budget = 20000000LL,
                                bool allow_interior_sinks = false) {
    if (!config.with_time_window) {
        throw std::logic_error(
            "ng_cyclic_optimum needs with_time_window: the horizon is what makes the walk "
            "enumeration finite");
    }

    const InstanceDraw draw = draw_instance(config);
    const std::map<size_t, std::set<size_t>> neighborhoods = ng_neighborhoods(config.num_nodes);
    const std::set<size_t> revisitable = ng_revisitable_nodes(config);
    const std::set<size_t> sinks(draw.sinks.begin(), draw.sinks.end());

    std::vector<std::vector<const InstanceDraw::Arc*>> out_arcs(config.num_nodes);
    for (const auto& arc : draw.arcs) {
        out_arcs[arc.origin].push_back(&arc);
    }

    double best = std::numeric_limits<double>::infinity();

    struct State {
            size_t node;
            double cost;
            double time;
            double load;
            std::set<size_t> memory;
    };
    std::vector<State> stack;
    stack.push_back({0, 0.0, 0.0, 0.0, {}});

    long long expanded = 0;
    while (!stack.empty()) {
        State state = std::move(stack.back());
        stack.pop_back();

        if (++expanded > state_budget) {
            throw std::logic_error(
                "ng_cyclic_optimum exhausted its state budget; the instance is too large to serve "
                "as an oracle. Shrink num_nodes, density or back_arc_density");
        }

        if (sinks.contains(state.node)) {
            best = std::min(best, state.cost);
            if (!allow_interior_sinks) {
                continue;  // a path ends at the first sink it reaches
            }
            // else: fall through -- a sink may still have outgoing arcs to a further sink
        }

        // The memory carried when leaving state.node, as the extension function computes it.
        std::set<size_t> memory_on_leaving;
        if (auto it = neighborhoods.find(state.node); it != neighborhoods.end()) {
            for (const size_t remembered : state.memory) {
                if (it->second.contains(remembered)) {
                    memory_on_leaving.insert(remembered);
                }
            }
        }
        memory_on_leaving.insert(state.node);

        for (const auto* arc : out_arcs[state.node]) {
            if (config.with_ng_path && memory_on_leaving.contains(arc->destination) &&
                !revisitable.contains(arc->destination)) {
                continue;  // ng-infeasible: this half already remembers the destination
            }
            const double time = state.time + arc->time;
            if (time > draw.windows.at(arc->destination).second) {
                continue;
            }
            const double load = state.load + arc->load;
            if (config.with_capacity && load > draw.capacity) {
                continue;
            }
            stack.push_back(
                {arc->destination, state.cost + arc->cost, time, load, memory_on_leaving});
        }
    }

    return best;
}

}  // namespace test_util
