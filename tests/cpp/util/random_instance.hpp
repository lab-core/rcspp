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

        /// @brief Adds an ng-path component: `NgPathExtensionFunction` +
        ///        `IntersectionFeasibilityFunction(forbidden)`, i.e. the shape that makes
        ///        `MergeRule::Disjoint` dispatch at the join.
        ///
        /// Only `build_ng_instance` reads this; the single-type `build_instance` ignores it,
        /// because an ng component needs a container type slot the one-type pack does not have.
        ///
        /// On an **acyclic** instance this can never *reject* anything -- no path revisits a node,
        /// and the two halves of a join are node-disjoint by construction. It is still worth
        /// setting there, to exercise the component in both directions; but a test that means to
        /// check ng *semantics* must also set @c back_arc_density.
        ///
        /// When false, `build_ng_instance` still registers the component, with empty forbidden
        /// sets. That keeps the label width identical between the two settings, so a cost
        /// difference is attributable to ng and not to the shape of the label.
        bool with_ng_path = false;

        /// @brief Probability of an arc `j -> i` with `j > i`, which introduces cycles.
        ///
        /// 0 keeps the generator's DAG guarantee -- which `brute_force_optimum` depends on.
        /// Above 0 the graph is cyclic and the ng component becomes load-bearing.
        ///
        /// **Requires @c with_time_window**, and `draw_instance` refuses without it: on a cyclic
        /// graph the window horizon is the *only* thing bounding path length, so the forward
        /// search would not terminate. Every arc consumes at least one time unit, so a finite
        /// horizon caps the number of arcs on a path and the search is finite.
        ///
        /// **Pair it with @c mixed_sign_costs**, which is not merely allowed but necessary for
        /// the cyclic tier to mean anything. With non-negative costs a shortest path never
        /// revisits a node, so ng-feasibility cannot change the optimum and a cyclic instance
        /// tests nothing an acyclic one does not. Negative reduced costs are also the situation
        /// the ng-path relaxation exists for.
        ///
        /// A negative *cycle* is therefore possible and is fine: the window bounds path length,
        /// so the optimum stays finite. Measured on the tier-2 configuration, ng cuts the
        /// optimum on 5 of 8 seeds.
        double back_arc_density = 0.0;

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
    text += config.with_ng_path ? " ng=yes" : " ng=no";
    if (config.back_arc_density > 0.0) {
        text += " back_arcs=" + std::to_string(config.back_arc_density);
    }
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
    // Both guards live here rather than in the tests, because the tests are where someone will
    // get this wrong and the generator is the one place that can stop them.
    // The one illegal combination, refused here rather than documented, because the tests are
    // where someone will get this wrong and the generator is the one place that can stop them.
    //
    // Note what is NOT guarded: mixed_sign_costs with back arcs. An earlier draft forbade it on
    // the grounds that a negative cycle leaves the forward reference with no finite answer. That
    // is false once a window is present -- the horizon caps the number of arcs on a path, so the
    // optimum is finite whatever the arc signs -- and forbidding it would have made the cyclic
    // tier vacuous, since with non-negative costs a shortest path never revisits a node.
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
                draw.arcs.push_back({i, j, next_cost(), time_draw(rng), load_draw(rng)});
            }
        }
    }

    // Back arcs, drawn last so that adding them does not shift the forward arcs' random draw:
    // for a given seed, `back_arc_density = 0` and `> 0` produce the same forward arcs.
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

    // Earliest arrival along the spine, which every window has to admit or the instance is
    // trivially infeasible and tests nothing.
    std::vector<double> earliest(config.num_nodes, 0.0);
    for (const auto& arc : draw.arcs) {
        if (arc.destination == arc.origin + 1) {
            earliest[arc.destination] = earliest[arc.origin] + arc.time;
        }
    }
    // `earliest` is the spine walk, so back arcs do not enter it -- which is what keeps the
    // windows identical between the acyclic and cyclic settings of one seed.
    //
    // On a cyclic instance the window is also the only thing bounding path length, and it has to
    // leave room for at least one detour or no path can ever revisit a node and ng cannot bind.
    // `cyclic_slack` widens every window by a constant factor for exactly that reason.
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

/// @brief The ng variant of @c GeneratedInstance: a two-slot pack with a container component.
struct NgGeneratedInstance {
        std::unique_ptr<rcspp::ResourceGraph<rcspp::RealResource, rcspp::SizeTBitsetResource>>
            graph;

        /// @brief Component index of the clock, *within the RealResource slot*.
        ///
        /// The ng component lives in the other type slot, so it does not shift this index and the
        /// numbering matches @c GeneratedInstance's exactly.
        size_t clock_index = 0;

        /// @brief `R`, the clock's finite maximum; `H` is derived from it as `R / 2`.
        double clock_upper_bound = 0.0;

        /// @brief Whether the clock is a resource the half-way bound can legitimately read.
        bool clock_is_usable = false;
};

/// @brief The ng neighborhood of each node: a window of node ids centred on it.
///
/// A *window* rather than the whole node set, because that is what makes the relaxation an ng-path
/// relaxation rather than plain elementarity -- a cycle wider than the window is still allowed.
/// The width is what decides whether ng actually binds, so
/// `NgPathActuallyBindsOnAtLeastOneCyclicInstance` exists to fail if it is ever widened past the
/// point of usefulness.
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
/// Sharing @c draw_instance is what makes the comparison honest: for a given seed the two
/// builders see identical arcs, windows and capacities, so the only difference is the presence of
/// the ng component.
///
/// The ng arcs carry **no per-arc set data**. Step 4's accepted narrowing means
/// @c NgPathExtensionFunction derives the node it adds from the arc's own endpoints, so supplying
/// an origin singleton here would be ignored -- and supplying anything else would be silently
/// ignored too. An empty set is the honest initializer.
///
/// @param config The instance to build. @c with_ng_path decides whether the ng component
///               *constrains* anything; the component is registered either way, so the label
///               width does not depend on the flag.
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

    // The ng component, always registered so the label width does not depend on the flag.
    //
    // `forbidden_by_node[v] = {v}` is the ng-route condition: a label arriving at v is infeasible
    // if v is already in the memory its own half accumulated. With the flag off the forbidden sets
    // are empty, so IntersectionFeasibilityFunction short-circuits and the component is inert --
    // present, extended in both directions, constraining nothing.
    std::map<size_t, std::set<size_t>> forbidden_by_node;
    if (config.with_ng_path) {
        for (size_t node = 0; node < config.num_nodes; ++node) {
            forbidden_by_node[node] = {node};
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
        // See build_instance: the tuple carries the cost COMPONENT, the last argument the
        // ORIGINAL weight. `arc_cost_offset` separates them.
        const double original_weight = arc.cost + config.arc_cost_offset;
        const std::set<size_t> no_arc_set;  // see the class comment: the arc value is ignored
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
/// An independent oracle: it shares no code with either algorithm, so an error common to both --
/// a dominance rule that discards something it should not -- still shows up here. Exponential, so
/// the suite only calls it on the small, sparse end of the sweep.
///
/// **A path ends at the first sink it reaches**, which is what `Node::sink` means and what every
/// search in the library does since D5. This oracle used to walk *through* a sink and keep going,
/// on the reasoning that a sink may have outgoing arcs to a further sink -- which was the older,
/// more permissive model, and stopped being the one the algorithms solve. With one sink the two
/// enumerations cannot differ (the sink is the highest-numbered node and has no out-arcs), which is
/// why the discrepancy survived: it needs `num_sinks >= 2` *and* a reason to prefer the longer
/// walk, i.e. `mixed_sign_costs`. `JoinOptimality.TheOracleEndsAPathAtItsFirstSink` is the case
/// that shows the two apart.
///
/// @param config               The instance's config; the same draw is replayed.
/// @param allow_interior_sinks Whether a walk may pass THROUGH a sink and continue. @c false --
///                             the default, and the model the library solves. @c true keeps the
///                             older enumeration, retained so the two can be compared and so the
///                             difference cannot be quietly designed away.
/// @return The optimal cost, or infinity when no feasible path exists.
inline double brute_force_optimum(const InstanceConfig& config, bool allow_interior_sinks = false) {
    // The walk below has no visited set: it relies on arcs going forwards in node order. On a
    // cyclic instance it would not terminate, so refuse rather than hang or silently
    // under-enumerate.
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
            if (!allow_interior_sinks) {
                continue;  // a path ends at the first sink it reaches
            }
            // else: fall through -- a sink may still have outgoing arcs to a further sink
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

/// @brief The true optimum of the **cyclic ng** model, by enumerating walks.
///
/// A second oracle, needed because @c brute_force_optimum cannot serve here: it enumerates simple
/// paths by walking forwards in node order, which a cyclic instance breaks. This one allows
/// revisits and carries the ng memory explicitly, exactly as @c NgPathExtensionFunction computes
/// it -- `(memory & neighborhood[node_left]) | {node_left}` -- so it is an independent statement
/// of what the model means rather than a second copy of the search.
///
/// It shares no code with either algorithm, which is the point: on the cyclic ng model the
/// forward search is **not** a valid reference (see the note in `test_equivalence.hpp`), so a
/// bidirectional-versus-forward comparison cannot settle correctness there. This can.
///
/// Termination rests on the window: every arc consumes at least one time unit and the horizon is
/// finite, so the number of feasible walks is finite. @p state_budget guards against a
/// configuration where "finite" is still far too large -- it **throws** rather than returning a
/// truncated answer, because a silently under-enumerated oracle is worse than none.
///
/// @param config              The instance. Must set @c with_time_window; @c with_capacity is
///                            honoured.
/// @param state_budget        Maximum walk states to expand before giving up.
/// @param allow_interior_sinks Whether a walk may pass THROUGH a sink and continue.
///                            @c false -- the default, and the model the library solves -- ends a
///                            path at the first sink it reaches, which is what `Node::sink` means
///                            and what every search in the library does. @c true keeps the older,
///                            more permissive enumeration, retained so the two can be compared and
///                            so the difference cannot be quietly designed away.
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

        // The memory a label carries when it LEAVES state.node -- the node being left is the
        // arc's origin going forwards, which is what the extension function uses.
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
            if (config.with_ng_path && memory_on_leaving.contains(arc->destination)) {
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
