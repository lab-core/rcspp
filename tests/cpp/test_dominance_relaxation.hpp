// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// `dominance_ignored_components`: relaxing dominance per solve, on one graph.
//
// Relaxing a resource's dominance used to need a second graph, built with a
// `TrivialDominanceFunction` for that resource. The parameter must give the same search: the same
// labels extended and the same columns, forward and backward, for every labeling algorithm.

#include <gtest/gtest.h>

#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "heuristics/pricing_model.hpp"
#include "rcspp/rcspp.hpp"
#include "util/benchmark_constants.hpp"
#include "util/column_validator.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace dominance_relaxation_test {

using heuristics::PricingGraph;
using heuristics::PricingLC;
using heuristics::PricingModel;

/// @brief Component numbers in the pricing model: cost, time, load, then the ng memory.
constexpr size_t kTime = 1;
constexpr size_t kLoad = 2;
constexpr size_t kNgMemory = 3;

/// @brief What a solve did and found, in a form two solves can be compared by.
struct Outcome {
        size_t extended = 0;
        std::map<std::vector<size_t>, double> columns;
        bool bounded = false;
};

template <template <typename, typename> class Algo>
Outcome solve(PricingModel& model, AlgorithmBaseParams params) {
    auto algorithm = model.graph().create_algorithm<Algo>(params.with_container(PricingLC()));
    const auto result = model.graph().solve(algorithm.get(), -1e-6);
    Outcome outcome;
    outcome.extended = algorithm->get_number_of_extended_labels();
    outcome.bounded = result.bounded_by_half_way;
    for (const auto& solution : result.solutions) {
        outcome.columns.emplace(solution.path_arc_ids, solution.cost);
    }
    const auto issues = test_util::validate_columns(model.graph(), result.solutions);
    EXPECT_TRUE(issues.empty()) << test_util::describe(issues);
    return outcome;
}

/// @brief Params for a bidirectional run clocked on time.
inline AlgorithmBaseParams bidirectional(double horizon) {
    AlgorithmBaseParams params;
    params.direction = SearchDirection::Bidirectional;
    params.critical_resource_index = 1;
    params.half_way_point = horizon / 2.0;
    return params;
}

/// @brief Runs @p Algo on the two-graph relaxation and on the parameter, and compares.
///
/// @return Whether the relaxation changed the search at all (fewer labels extended than the exact
///         solve on the same graph), so a caller can check the comparison was not vacuous.
template <template <typename, typename> class Algo>
bool expect_same_search(const Instance& instance, const std::map<size_t, double>& duals,
                        heuristics::ModelOptions relaxed_model, size_t component,
                        AlgorithmBaseParams params, const std::string& where) {
    PricingModel two_graphs(instance, duals, relaxed_model);
    auto plain_options = relaxed_model;
    plain_options.relax_load_dominance = false;
    plain_options.relax_ng_dominance = false;
    PricingModel one_graph(instance, duals, plain_options);

    const auto reference = solve<Algo>(two_graphs, params);
    const auto exact = solve<Algo>(one_graph, params);
    params.dominance_ignored_components = {component};
    const auto relaxed = solve<Algo>(one_graph, params);

    EXPECT_EQ(relaxed.extended, reference.extended) << where;
    EXPECT_EQ(relaxed.bounded, reference.bounded) << where;
    EXPECT_EQ(relaxed.columns.size(), reference.columns.size()) << where;
    EXPECT_TRUE(relaxed.columns == reference.columns)
        << where << ": the relaxed solve returned different columns";
    return relaxed.extended < exact.extended;
}

}  // namespace dominance_relaxation_test

/// @brief The parameter gives the same search as a second graph with a trivial dominance, for the
///        load and for the ng memory, forward (Simple, Pushing) and in both directions
///        (Bidirectional).
TEST(DominanceRelaxation, MatchesATrivialDominanceFunctionOnASecondGraph) {
    namespace dr = dominance_relaxation_test;
    namespace bb = bidirectional_benchmark;

    size_t changed = 0;
    size_t compared = 0;
    for (const std::string name : {"R101_25", "C101_25", "RC201_12"}) {
        const auto instance = bb::load(name);
        const auto duals = bb::synthetic_duals(instance, bb::kDualAlpha);
        const double horizon = static_cast<double>(instance.get_depot_customer().due_time);

        const std::vector<std::pair<heuristics::ModelOptions, size_t>> cases{
            {{.relax_load_dominance = true}, dr::kLoad},
            {{.relaxation = heuristics::Relaxation::Ng, .relax_load_dominance = true}, dr::kLoad},
            {{.relaxation = heuristics::Relaxation::Ng, .relax_ng_dominance = true}, dr::kNgMemory},
        };
        for (const auto& [model, component] : cases) {
            const std::string where = name + " " + model.tag();
            SCOPED_TRACE(where);
            changed += dr::expect_same_search<SimpleDominanceAlgorithm>(instance,
                                                                        duals,
                                                                        model,
                                                                        component,
                                                                        {},
                                                                        where + " simple");
            changed += dr::expect_same_search<PushingDominanceAlgorithm>(instance,
                                                                         duals,
                                                                         model,
                                                                         component,
                                                                         {},
                                                                         where + " pushing");
            changed += dr::expect_same_search<SimpleDominanceAlgorithm>(instance,
                                                                        duals,
                                                                        model,
                                                                        component,
                                                                        dr::bidirectional(horizon),
                                                                        where + " bidirectional");
            compared += 3;
        }
    }
    // Each relaxation must actually prune: equal results from two searches that ignore the
    // parameter would pass the comparisons above.
    EXPECT_GT(changed, compared / 2) << changed << " of " << compared
                                     << " relaxed solves extended fewer labels than the exact one";
}

/// @brief Relaxing the clock turns the half-way bound off, and says why.
TEST(DominanceRelaxation, RelaxingTheClockTurnsTheHalfWayBoundOff) {
    namespace dr = dominance_relaxation_test;
    namespace bb = bidirectional_benchmark;
    const auto instance = bb::load("R101_25");
    const auto duals = bb::synthetic_duals(instance, bb::kDualAlpha);
    heuristics::PricingModel model(instance, duals, {});

    auto params = dr::bidirectional(model.horizon());
    params.dominance_ignored_components = {dr::kTime};
    auto algorithm = model.graph().create_algorithm<SimpleDominanceAlgorithm>(
        params.with_container(dr::PricingLC()));
    const auto result = model.graph().solve(algorithm.get(), -1e-6);

    EXPECT_FALSE(result.bounded_by_half_way);
    EXPECT_NE(result.half_way_off_reason.find("dominance_ignored_components"), std::string::npos)
        << result.half_way_off_reason;

    // Relaxing another component leaves the bound on.
    params.dominance_ignored_components = {dr::kLoad};
    auto other = model.graph().create_algorithm<SimpleDominanceAlgorithm>(
        params.with_container(dr::PricingLC()));
    EXPECT_TRUE(model.graph().solve(other.get(), -1e-6).bounded_by_half_way);
}

/// @brief A component the model does not have is refused, not silently ignored.
TEST(DominanceRelaxation, AComponentTheModelDoesNotHaveIsRefused) {
    namespace dr = dominance_relaxation_test;
    namespace bb = bidirectional_benchmark;
    const auto instance = bb::load("R101_25");
    const auto duals = bb::synthetic_duals(instance, bb::kDualAlpha);
    heuristics::PricingModel model(instance, duals, {});  // cost, time, load: components 0 to 2

    AlgorithmBaseParams params;
    params.dominance_ignored_components = {dr::kNgMemory};
    auto simple = model.graph().create_algorithm<SimpleDominanceAlgorithm>(
        params.with_container(dr::PricingLC()));
    EXPECT_THROW(static_cast<void>(model.graph().solve(simple.get(), -1e-6)),
                 std::invalid_argument);

    auto bidirectional_params = dr::bidirectional(model.horizon());
    bidirectional_params.dominance_ignored_components = {dr::kNgMemory};
    auto bidirectional = model.graph().create_algorithm<SimpleDominanceAlgorithm>(
        bidirectional_params.with_container(dr::PricingLC()));
    EXPECT_THROW(static_cast<void>(model.graph().solve(bidirectional.get(), -1e-6)),
                 std::invalid_argument);
}

/// @brief A relaxed solve is flagged as possibly non-optimal.
TEST(DominanceRelaxation, ARelaxedSolveCouldBeNonOptimal) {
    AlgorithmBaseParams params;
    EXPECT_FALSE(params.could_be_non_optimal());
    params.dominance_ignored_components = {2};
    EXPECT_TRUE(params.could_be_non_optimal());
}
