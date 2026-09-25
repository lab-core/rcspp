// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Every heuristic configuration the heuristic study runs returns only real columns.
//
// Before any heuristic is measured (test_heuristic_sweep.hpp), each configuration is run on
// generated instances whose optimum an oracle knows, and every column it returns is replayed by
// util/column_validator.hpp. A heuristic may return worse columns than an exact algorithm, or none;
// it may never return an infeasible one, a mispriced one, or one cheaper than the optimum. The
// exact configurations must also reach the optimum.
//
// This covers the knobs that make the bidirectional search inexact (quotas, join budgets, pair
// caps, early stops), the dives, and `DiversificationSearch` wrapped around a bidirectional
// solver, which nothing tested before (E2's precondition).

#include <gtest/gtest.h>

#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/column_validator.hpp"
#include "util/random_instance.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace heuristic_correctness_test {

constexpr double kTolerance = 1e-6;

/// @brief The configurations under test, for one resource pack.
template <typename... Ts>
struct Configurations {
        using RC = ResourceTypeComposition<Ts...>;
        using LC = LabelList<RC>;
        using Graph = ResourceGraph<Ts...>;
        using Ptr = std::unique_ptr<Algorithm<RC, LC>>;
        using Maker = std::function<Ptr(Graph&)>;

        struct Entry {
                std::string name;
                Maker maker;
                bool exact = false;
        };

        template <template <typename, typename> class Algo>
        static Maker make(AlgorithmBaseParams params) {
            return [params](Graph& graph) -> Ptr {
                return graph.template create_algorithm<Algo>(params.with_container(LC()));
            };
        }

        static Maker diversification(AlgorithmBaseParams outer, Maker inner) {
            return [outer, inner = std::move(inner)](Graph& graph) -> Ptr {
                return std::make_unique<DiversificationSearch<RC, LC>>(
                    &graph.get_resource_factory(),
                    outer.with_container(LC()),
                    inner ? inner(graph) : nullptr);
            };
        }

        /// @param clock   The clock's index in the real slot.
        /// @param horizon The clock's range; `H` is half of it (0 turns the bound off).
        static std::vector<Entry> all(size_t clock, double horizon) {
            auto bidirectional = [&](auto&& tweak) {
                AlgorithmBaseParams params;
                params.direction = SearchDirection::Bidirectional;
                params.critical_resource_index = clock;
                params.half_way_point = horizon / 2.0;
                tweak(params);
                return make<SimpleDominanceAlgorithm>(params);
            };
            auto plain = [](auto&& tweak) {
                AlgorithmBaseParams params;
                tweak(params);
                return params;
            };
            const auto none = [](AlgorithmBaseParams&) {};

            std::vector<Entry> entries;
            entries.push_back({"simple", make<SimpleDominanceAlgorithm>(plain(none)), true});
            entries.push_back({"bidirectional", bidirectional(none), true});

            // Truncated forward labeling.
            for (const size_t quota : {1U, 2U}) {
                const auto params = plain(
                    [quota](AlgorithmBaseParams& p) { p.num_labels_to_extend_by_node = quota; });
                entries.push_back(
                    {"simple/q" + std::to_string(quota), make<SimpleDominanceAlgorithm>(params)});
                entries.push_back(
                    {"pushing/q" + std::to_string(quota), make<PushingDominanceAlgorithm>(params)});
                entries.push_back(
                    {"pulling/q" + std::to_string(quota), make<PullingDominanceAlgorithm>(params)});
            }

            // The bidirectional search made inexact, one knob at a time.
            for (const size_t quota : {1U, 2U, 5U}) {
                entries.push_back({"bidirectional/q" + std::to_string(quota),
                                   bidirectional([quota](AlgorithmBaseParams& p) {
                                       p.num_labels_to_extend_by_node = quota;
                                   })});
            }
            entries.push_back(
                {"bidirectional/q2-phases3", bidirectional([](AlgorithmBaseParams& p) {
                     p.num_labels_to_extend_by_node = 2;
                     p.num_max_phases = 3;
                 })});
            entries.push_back(
                {"bidirectional/join-budget3",
                 bidirectional([](AlgorithmBaseParams& p) { p.join_column_budget = 3; }),
                 true});  // the cheapest joined path is always kept
            entries.push_back(
                {"bidirectional/max-pairs5",
                 bidirectional([](AlgorithmBaseParams& p) { p.max_join_pairs = 5; })});
            entries.push_back({"bidirectional/timeout0",
                               bidirectional([](AlgorithmBaseParams& p) { p.timeout_s = 0.0; })});
            entries.push_back(
                {"bidirectional/timeout0-no-join", bidirectional([](AlgorithmBaseParams& p) {
                     p.timeout_s = 0.0;
                     p.join_after_early_stop = false;
                 })});
            entries.push_back(
                {"bidirectional/max-iterations3",
                 bidirectional([](AlgorithmBaseParams& p) { p.max_iterations = 3; })});
            entries.push_back(
                {"bidirectional/stop-after2", bidirectional([](AlgorithmBaseParams& p) {
                     p.stop_after_X_solutions = 2;
                     p.return_dominated_solutions = true;
                 })});

            // The dives.
            entries.push_back({"greedy/20", make<GreedyAlgorithm>(plain([](AlgorithmBaseParams& p) {
                                   p.stop_after_X_solutions = 20;
                               }))});
            entries.push_back({"tabu/50",
                               make<TabuSearchAlgorithm>(
                                   plain([](AlgorithmBaseParams& p) { p.max_iterations = 50; }))});
            entries.push_back({"improving-tabu/50",
                               make<ImprovingTabuSearch>(
                                   plain([](AlgorithmBaseParams& p) { p.max_iterations = 50; }))});

            // Diversification, around its default Greedy and around bidirectional solvers.
            const auto outer = plain([](AlgorithmBaseParams& p) {
                p.max_iterations = 10;
                p.stop_after_X_solutions = 10;
            });
            entries.push_back({"diversification/greedy", diversification(outer, nullptr)});
            entries.push_back({"diversification/bidirectional",
                               diversification(outer, bidirectional([](AlgorithmBaseParams& p) {
                                                   p.release_after_solve =
                                                       false;  // as the default inner has
                                               }))});
            entries.push_back({"diversification/bidirectional-q2",
                               diversification(outer, bidirectional([](AlgorithmBaseParams& p) {
                                                   p.release_after_solve = false;
                                                   p.num_labels_to_extend_by_node = 2;
                                                   p.join_column_budget = 3;
                                               }))});
            return entries;
        }
};

/// @brief Runs every configuration on graphs from @p build and checks it against @p optimum.
///
/// @param build   Returns a fresh graph each call (a solve modifies the graph in place).
/// @param optimum The oracle's optimum.
/// @param where   The instance, for failure messages.
/// @return How many columns were checked.
template <typename Configs, typename Build>
size_t check_all(const Build& build, double optimum, const std::string& where, size_t clock,
                 double horizon) {
    size_t checked = 0;
    for (const auto& entry : Configs::all(clock, horizon)) {
        SCOPED_TRACE(entry.name);
        auto graph = build();
        auto algorithm = entry.maker(*graph);
        const auto result = graph->solve(algorithm.get());

        const auto issues = test_util::validate_columns(*graph, result.solutions, kTolerance);
        EXPECT_TRUE(issues.empty()) << entry.name << " " << where << test_util::describe(issues);
        checked += result.solutions.size();

        if (result.solutions.empty()) {
            // An exact configuration finds a column whenever one exists.
            EXPECT_FALSE(entry.exact && std::isfinite(optimum)) << entry.name << " " << where;
            continue;
        }
        const double best = result.solutions.front().cost;
        EXPECT_GE(best, optimum - kTolerance)
            << entry.name << " returned a column cheaper than the optimum: " << where;
        if (entry.exact) {
            EXPECT_NEAR(best, optimum, kTolerance) << entry.name << " " << where;
        }
    }
    return checked;
}

}  // namespace heuristic_correctness_test

/// @brief Acyclic instances with a clock, a capacity and reduced costs, against brute force.
TEST(HeuristicCorrectness, EveryConfigurationReturnsRealColumnsOnAcyclicInstances) {
    namespace hc = heuristic_correctness_test;
    using Configs = hc::Configurations<RealResource>;

    size_t checked = 0;
    for (unsigned seed = 0; seed < 12; ++seed) {
        test_util::InstanceConfig config;
        config.num_nodes = 9 + (seed % 4);
        config.density = 0.5;
        config.with_time_window = true;
        config.window_slack = 0.5 + (0.5 * (seed % 3));
        config.with_capacity = (seed % 2) == 0;
        config.capacity_binds = (seed % 4) == 0;
        config.mixed_sign_costs = true;
        config.num_sinks = 1 + (seed % 2);
        config.span_scaled_shortcuts = (seed % 3) == 0;
        config.seed = seed;
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const double optimum = test_util::brute_force_optimum(config);
        const auto probe = test_util::build_instance(config);
        checked += hc::check_all<Configs>([&] { return test_util::build_instance(config).graph; },
                                          optimum,
                                          where,
                                          probe.clock_index,
                                          probe.clock_upper_bound);
    }
    EXPECT_GT(checked, 0U) << "no configuration returned a column, so nothing was validated";
}

/// @brief Cyclic ng instances, against the ng oracle.
TEST(HeuristicCorrectness, EveryConfigurationReturnsRealColumnsOnCyclicNgInstances) {
    namespace hc = heuristic_correctness_test;
    using Configs = hc::Configurations<RealResource, SizeTBitsetResource>;

    size_t checked = 0;
    for (unsigned seed = 0; seed < 6; ++seed) {
        test_util::InstanceConfig config;
        config.num_nodes = 8;
        config.density = 0.6;
        config.back_arc_density = 0.3;
        config.mixed_sign_costs = true;
        config.with_time_window = true;
        config.with_ng_path = true;
        config.seed = seed;
        const std::string where = test_util::describe(config);
        SCOPED_TRACE(where);

        const double optimum = test_util::ng_cyclic_optimum(config);
        const auto probe = test_util::build_ng_instance(config);
        checked +=
            hc::check_all<Configs>([&] { return test_util::build_ng_instance(config).graph; },
                                   optimum,
                                   where,
                                   probe.clock_index,
                                   probe.clock_upper_bound);
    }
    EXPECT_GT(checked, 0U) << "no configuration returned a column, so nothing was validated";
}
