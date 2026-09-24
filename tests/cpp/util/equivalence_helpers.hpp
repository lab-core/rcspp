// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Shared machinery for the equivalence suites: build a model, solve it, replay every returned
// path through the real Extender and report what is wrong with it.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/random_instance.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace equivalence_test {

using Composed = ResourceTypeComposition<RealResource>;
using Graph = ResourceGraph<RealResource>;

constexpr double kTolerance = 1e-9;

/// @brief A finite upper bound no path reaches.
///
/// With an infinite bound the join prunes against the incumbent, so it would drop most pairs and
/// the sweep would barely exercise it. A finite one makes it return every pair it admits.
constexpr double kNonBindingUpperBound = 1e9;

/// @brief Checks the pool's reference counts after a solve that kept its labels.
///
/// With `release_after_solve` on the pool is empty after a solve, so the check would pass on
/// nothing; the callers turn it off.
inline void expect_consistent_ref_counts(const LabelPool<Composed>& pool) {
    EXPECT_GT(pool.get_nb_total_labels(), 0U) << "the pool is empty, so the check tests nothing";
    EXPECT_TRUE(pool.check_ref_count_consistency())
        << "reference counts must survive two searches and a join";
}

/// @brief A solve and the graph it ran on, kept together so the paths can be replayed afterwards.
struct Run {
        std::unique_ptr<Graph> graph;
        SolveResult result;
        bool bounded = false;

        [[nodiscard]] double best_cost() const {
            return result.solutions.empty() ? std::numeric_limits<double>::infinity()
                                            : result.solutions.front().cost;
        }
};

/// @brief The reference: an ordinary forward search.
inline Run solve_forward(const test_util::InstanceConfig& config) {
    Run run;
    auto built = test_util::build_instance(config);
    run.graph = std::move(built.graph);
    run.result = run.graph->solve<SimpleDominanceAlgorithm>(AlgorithmBaseParams{});
    return run;
}

/// @brief The candidate: a bidirectional search.
///
/// @param config       The instance to build.
/// @param with_bound   When false the half-way point is 0 and the bound starts disabled, which
///                     separates bound bugs from join bugs.
inline Run solve_bidirectional(const test_util::InstanceConfig& config, bool with_bound) {
    Run run;
    auto built = test_util::build_instance(config);
    run.graph = std::move(built.graph);

    AlgorithmParams<LabelList<Composed>> params;
    params.critical_resource_index = built.clock_index;
    params.half_way_point = with_bound ? built.clock_upper_bound / 2.0 : 0.0;
    params.release_after_solve = false;

    auto algorithm =
        run.graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    run.result = run.graph->solve(algorithm.get(), kNonBindingUpperBound);
    run.bounded = algorithm->bounded_by_half_way();
    expect_consistent_ref_counts(algorithm->get_label_pool());
    return run;
}

/// @brief What a bounded run varies. Defaults reproduce an unbounded run exactly.
struct BoundedOptions {
        /// @brief The caller's `upper_bound`: solutions costing at least this are not returned.
        double upper_bound = std::numeric_limits<double>::infinity();

        /// @brief Whether the search prunes against the incumbent (a completion bound for the
        ///        bidirectional search).
        bool prune_based_on_upper_bound = false;

        /// @brief Whether the half-way bound is in force (bidirectional only).
        bool with_half_way_bound = true;

        /// @brief Whether `solve()` preprocesses. With a binding upper bound this can strip
        ///        every arc from the graph.
        bool preprocess = true;
};

/// @brief The reference, under a caller-supplied upper bound.
inline Run solve_forward_bounded(const test_util::InstanceConfig& config,
                                 const BoundedOptions& options) {
    Run run;
    auto built = test_util::build_instance(config);
    run.graph = std::move(built.graph);

    AlgorithmParams<LabelList<Composed>> params;
    params.prune_based_on_upper_bound_ = options.prune_based_on_upper_bound;
    run.result =
        run.graph->solve<SimpleDominanceAlgorithm>(options.upper_bound, params, options.preprocess);
    return run;
}

/// @brief The candidate, under a caller-supplied upper bound.
inline Run solve_bidirectional_bounded(const test_util::InstanceConfig& config,
                                       const BoundedOptions& options) {
    Run run;
    auto built = test_util::build_instance(config);
    run.graph = std::move(built.graph);

    AlgorithmParams<LabelList<Composed>> params;
    params.critical_resource_index = built.clock_index;
    params.half_way_point = options.with_half_way_bound ? built.clock_upper_bound / 2.0 : 0.0;
    params.prune_based_on_upper_bound_ = options.prune_based_on_upper_bound;
    // The completion bounds must relax on the slot the labels accumulate.
    params.heuristic_cost_index = 0;
    params.release_after_solve = false;

    auto algorithm =
        run.graph->create_algorithm<BidirectionalAlgoBound<RealResource>::Algo>(params);
    run.result = run.graph->solve(algorithm.get(), options.upper_bound, options.preprocess);
    run.bounded = algorithm->bounded_by_half_way();
    expect_consistent_ref_counts(algorithm->get_label_pool());
    return run;
}

/// @brief Replays one path through the model, returning what is wrong with it, or "".
///
/// Extends a fresh source resource arc by arc through each arc's extender, requires every node
/// along the way to be feasible, and requires the replayed cost to be the reported one.
inline std::string path_problem(const rcspp::Graph<Composed>& graph, const Solution& solution) {
    if (solution.path_arc_ids.empty()) {
        return "empty path";
    }

    const auto* first = graph.get_arc(solution.path_arc_ids.front());
    if (first == nullptr) {
        return "unknown arc id";
    }
    if (!first->origin->source) {
        return "path does not start at a source";
    }

    auto current = std::make_unique<Resource<Composed>>(*first->origin->resource);
    const Arc<Composed>* previous = nullptr;

    for (size_t arc_id : solution.path_arc_ids) {
        const auto* arc = graph.get_arc(arc_id);
        if (arc == nullptr) {
            return "unknown arc id";
        }
        if (previous != nullptr && previous->destination != arc->origin) {
            return "path is not contiguous at arc " + std::to_string(arc_id);
        }
        if (arc->extender == nullptr) {
            return "arc has no extender";
        }

        auto extended = std::make_unique<Resource<Composed>>(*arc->destination->resource);
        arc->extender->extend(*current, extended.get());
        if (!extended->is_feasible()) {
            return "path is infeasible at node " + std::to_string(arc->destination->id);
        }
        current = std::move(extended);
        previous = arc;
    }

    if (!previous->destination->sink) {
        return "path does not end at a sink";
    }
    if (std::abs(current->get_cost() - solution.cost) > kTolerance) {
        return "path costs " + std::to_string(current->get_cost()) + " but is reported at " +
               std::to_string(solution.cost);
    }
    return "";
}

/// @brief Replays every returned path and reports the first problem found.
inline std::string any_path_problem(const Run& run) {
    for (const auto& solution : run.result.solutions) {
        const std::string problem = path_problem(*run.graph, solution);
        if (!problem.empty()) {
            return problem;
        }
    }
    return "";
}

/// @brief The configurations the sweep runs. Each row turns on one dimension, so a failure points
///        at that dimension; seeds differ per row.
inline std::vector<test_util::InstanceConfig> sweep() {
    std::vector<test_util::InstanceConfig> configs;

    // Cost only, sparse and dense. The clock is the cost slot, so the bound must switch itself off.
    configs.push_back({.num_nodes = 10, .density = 0.2, .seed = 1});
    configs.push_back({.num_nodes = 12, .density = 0.9, .seed = 2});

    // Time windows, loose and tight. Tight windows are where a wrong deadline shows.
    configs.push_back({.num_nodes = 14,
                       .density = 0.4,
                       .with_time_window = true,
                       .window_slack = 2.0,
                       .seed = 3});
    configs.push_back({.num_nodes = 14,
                       .density = 0.4,
                       .with_time_window = true,
                       .window_slack = 0.05,
                       .seed = 4});
    configs.push_back({.num_nodes = 24,
                       .density = 0.6,
                       .with_time_window = true,
                       .window_slack = 0.2,
                       .seed = 5});

    // Capacity, binding and not -- the Threshold clamp on its own.
    configs.push_back({.num_nodes = 12,
                       .density = 0.5,
                       .with_capacity = true,
                       .capacity_binds = true,
                       .seed = 6});
    configs.push_back({.num_nodes = 12,
                       .density = 0.5,
                       .with_capacity = true,
                       .capacity_binds = false,
                       .seed = 7});

    // Both, so the composition fans out over three components.
    configs.push_back({.num_nodes = 16,
                       .density = 0.5,
                       .with_time_window = true,
                       .window_slack = 0.5,
                       .with_capacity = true,
                       .capacity_binds = true,
                       .seed = 8});

    // Mixed-sign costs: the completion bound and the monotonicity fallback.
    configs.push_back({.num_nodes = 14, .density = 0.5, .mixed_sign_costs = true, .seed = 9});
    configs.push_back({.num_nodes = 18,
                       .density = 0.4,
                       .with_time_window = true,
                       .window_slack = 0.5,
                       .mixed_sign_costs = true,
                       .seed = 10});

    // Two sinks with different windows: per-node backward seeding.
    configs.push_back({.num_nodes = 15,
                       .density = 0.5,
                       .with_time_window = true,
                       .window_slack = 0.5,
                       .num_sinks = 2,
                       .seed = 11});
    configs.push_back({.num_nodes = 15,
                       .density = 0.5,
                       .with_capacity = true,
                       .capacity_binds = true,
                       .mixed_sign_costs = true,
                       .num_sinks = 2,
                       .seed = 12});

    // Shortcuts that cost what they skip, so optima use more of the clock and cross H at an
    // interior node: these are the rows that exercise the join.
    configs.push_back({.num_nodes = 14,
                       .density = 0.4,
                       .with_time_window = true,
                       .window_slack = 0.5,
                       .span_scaled_shortcuts = true,
                       .seed = 14});
    configs.push_back({.num_nodes = 16,
                       .density = 0.5,
                       .with_time_window = true,
                       .window_slack = 0.5,
                       .with_capacity = true,
                       .capacity_binds = true,
                       .span_scaled_shortcuts = true,
                       .seed = 15});
    configs.push_back({.num_nodes = 14,
                       .density = 0.5,
                       .with_capacity = true,
                       .capacity_binds = true,
                       .span_scaled_shortcuts = true,
                       .seed = 16});
    configs.push_back({.num_nodes = 18,
                       .density = 0.4,
                       .with_time_window = true,
                       .window_slack = 0.5,
                       .mixed_sign_costs = true,
                       .span_scaled_shortcuts = true,
                       .seed = 17});

    // The large end, to catch anything that only appears with many labels per node.
    configs.push_back({.num_nodes = 40,
                       .density = 0.3,
                       .with_time_window = true,
                       .window_slack = 0.3,
                       .with_capacity = true,
                       .capacity_binds = true,
                       .seed = 13});

    return configs;
}

}  // namespace equivalence_test
