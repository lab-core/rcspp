// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// E1: the sweep of heuristic configurations on fixed duals.
//
// For each pricing subproblem it runs the exact references, the truncated forward algorithms,
// the bidirectional search made inexact one knob at a time, the dives, and the diversification
// search, and emits one CSV row per run (see heuristics/sweep_harness.hpp). Nothing is asserted
// but column validity: the rows are the measurement.
//
// Disabled: each test takes from minutes to hours. Run one with, e.g.
//
// @code
//   set RCSPP_SWEEP_OUT=e1.csv
//   tests-rcspp --gtest_also_run_disabled_tests
//   --gtest_filter="HeuristicSweep.DISABLED_E1_RealDuals"
// @endcode
//
// in a Release build: Debug skews the ratios. `RCSPP_SWEEP_REPS` sets how many timed samples a
// configuration above one second gets (default 3). `RCSPP_SWEEP_DATASETS`, a comma-separated list
// of dataset names, restricts a test to those datasets.

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "heuristics/pricing_model.hpp"
#include "heuristics/sweep_harness.hpp"
#include "rcspp/rcspp.hpp"
#include "util/benchmark_constants.hpp"
#include "vrp/instance_reader.hpp"

namespace heuristic_sweep {

using namespace rcspp;       // NOLINT(google-build-using-namespace)
using namespace heuristics;  // NOLINT(google-build-using-namespace)
using Bidirectional = BidirectionalAlgoBound<RealResource>;

/// @brief One pricing subproblem's inputs.
struct Dataset {
        std::string name;
        Instance instance;
        std::map<size_t, double> duals;
};

/// @brief What to run on a dataset.
struct SweepOptions {
        /// @brief Also run the exact forward search, capped at this many seconds (0 skips it).
        double forward_exact_budget = 120.0;
        /// @brief Seconds allowed to a dive or a diversification run.
        double dive_budget = 30.0;
};

/// @brief Whether `RCSPP_SWEEP_DATASETS` selects @p name (it selects everything when unset).
inline bool selected(const std::string& name) {
    const std::string filter = environment("RCSPP_SWEEP_DATASETS");
    if (filter.empty()) {
        return true;
    }
    std::stringstream stream(filter);
    std::string item;
    while (std::getline(stream, item, ',')) {
        if (item == name) {
            return true;
        }
    }
    return false;
}

/// @brief Timed samples per configuration above one second: `RCSPP_SWEEP_REPS`, default 3.
inline size_t repetitions() {
    const std::string text = environment("RCSPP_SWEEP_REPS");
    return text.empty() ? 3 : std::max<size_t>(1, std::stoul(text));
}

/// @brief Runs a configuration, repeated so that its median time is a measurement.
inline RunRow measure(PricingModel& model, const std::string& dataset, const RunConfig& config,
                      const AlgorithmMaker& maker) {
    // Every configuration is repeated: E1 met multi-minute slow periods on this machine, so one
    // sample above a second is not a measurement. Under a second, 5 fresh repetitions; above,
    // the first run counts and `RCSPP_SWEEP_REPS` (default 3) samples are taken in all.
    RunRow row = run(model, dataset, config, maker, 1);
    if (row.seconds < 1.0) {
        row = run(model, dataset, config, maker, 5);
    } else if (const size_t total = repetitions(); total > 1) {
        const std::vector<double> first = row.samples;
        row = run(model, dataset, config, maker, total - 1);
        row.samples.insert(row.samples.end(), first.begin(), first.end());
        std::ranges::sort(row.samples);
        row.seconds = row.samples[row.samples.size() / 2];
    }
    emit(row);
    EXPECT_EQ(row.invalid, 0U) << dataset << " " << row.model << " " << config.algorithm << " "
                               << config.extra << ": " << row.first_issue;
    return row;
}

/// @brief Early stops: bidirectional with and without the join, and forward, stopped at fractions
///        of the exact bidirectional time.
///
/// @param model         The subproblem.
/// @param dataset       Its name.
/// @param exact_seconds The exact bidirectional solve's median time on this model.
/// @param fractions     The stops, as fractions of @p exact_seconds.
inline void sweep_early_stops(PricingModel& model, const std::string& dataset, double exact_seconds,
                              const std::vector<double>& fractions) {
    const auto base = guarded_params();
    const auto bidirectional = bidirectional_params(model.horizon());
    for (const double fraction : fractions) {
        const double timeout = std::max(1e-3, fraction * exact_seconds);
        for (const bool join_after_stop : {true, false}) {
            auto params = bidirectional;
            params.timeout_s = timeout;
            params.join_after_early_stop = join_after_stop;
            std::ostringstream extra;
            extra << "timeout-fraction=" << fraction;
            measure(model,
                    dataset,
                    {.algorithm = "bidirectional",
                     .timeout_s = timeout,
                     .join_after_stop = join_after_stop,
                     .extra = extra.str()},
                    make<Bidirectional::Algo>(params));
        }
        auto params = base;
        params.timeout_s = timeout;
        std::ostringstream extra;
        extra << "timeout-fraction=" << fraction;
        measure(model,
                dataset,
                {.algorithm = "simple", .timeout_s = timeout, .extra = extra.str()},
                make<SimpleDominanceAlgorithm>(params));
    }
}

/// @brief The whole E1 grid on one model.
inline void sweep(PricingModel& model, const std::string& dataset, const SweepOptions& options) {
    const double horizon = model.horizon();
    const auto base = guarded_params();
    const auto bidirectional = bidirectional_params(horizon);

    // References: exact bidirectional (the optimum), exact forward (the label pressure).
    const auto exact = measure(model,
                               dataset,
                               {.algorithm = "bidirectional"},
                               make<Bidirectional::Algo>(bidirectional));
    if (options.forward_exact_budget > 0.0) {
        auto params = base;
        params.timeout_s = options.forward_exact_budget;
        measure(model, dataset, {.algorithm = "simple"}, make<SimpleDominanceAlgorithm>(params));
    }

    // Setup cost (Q8): one label pop, then everything a solve does around it.
    for (const bool is_bidirectional : {false, true}) {
        auto params = is_bidirectional ? bidirectional : base;
        params.max_iterations = 1;
        const RunConfig config{.algorithm = is_bidirectional ? "bidirectional" : "simple",
                               .extra = "setup-probe"};
        if (is_bidirectional) {
            measure(model, dataset, config, make<Bidirectional::Algo>(params));
        } else {
            measure(model, dataset, config, make<SimpleDominanceAlgorithm>(params));
        }
    }

    const std::vector<size_t> quotas{1, 2, 5, 10, 50};

    // Truncated forward labeling.
    for (const size_t quota : quotas) {
        auto params = base;
        params.num_labels_to_extend_by_node = quota;
        measure(model,
                dataset,
                {.algorithm = "simple", .quota = quota},
                make<SimpleDominanceAlgorithm>(params));
        measure(model,
                dataset,
                {.algorithm = "pushing", .quota = quota},
                make<PushingDominanceAlgorithm>(params));
        measure(model,
                dataset,
                {.algorithm = "pulling", .quota = quota},
                make<PullingDominanceAlgorithm>(params));
    }

    // Truncated bidirectional labeling.
    for (const size_t quota : quotas) {
        auto params = bidirectional;
        params.num_labels_to_extend_by_node = quota;
        measure(model,
                dataset,
                {.algorithm = "bidirectional", .quota = quota},
                make<Bidirectional::Algo>(params));
    }

    // The join's budgets.
    for (const size_t quota : {static_cast<size_t>(MAX_INT), static_cast<size_t>(5)}) {
        for (const size_t budget : {20U, 200U}) {
            auto params = bidirectional;
            params.num_labels_to_extend_by_node = quota;
            params.join_column_budget = budget;
            measure(model,
                    dataset,
                    {.algorithm = "bidirectional", .quota = quota, .join_budget = budget},
                    make<Bidirectional::Algo>(params));
        }
    }
    for (const size_t pairs : {10'000U, 1'000'000U}) {
        auto params = bidirectional;
        params.max_join_pairs = pairs;
        measure(model,
                dataset,
                {.algorithm = "bidirectional", .max_pairs = pairs},
                make<Bidirectional::Algo>(params));
    }

    sweep_early_stops(model, dataset, exact.seconds, {0.05, 0.2, 0.5});

    // The dives.
    auto dive = base;
    dive.timeout_s = options.dive_budget;
    for (const size_t cap : {20U, 200U}) {
        auto params = dive;
        params.stop_after_X_solutions = cap;
        measure(model,
                dataset,
                {.algorithm = "greedy", .extra = "stop-after=" + std::to_string(cap)},
                make<GreedyAlgorithm>(params));
    }
    for (const size_t iterations : {100U, 1000U, 10'000U}) {
        auto params = dive;
        params.max_iterations = iterations;
        const std::string extra = "iterations=" + std::to_string(iterations);
        measure(model,
                dataset,
                {.algorithm = "tabu", .extra = extra},
                make<TabuSearchAlgorithm>(params));
        measure(model,
                dataset,
                {.algorithm = "improving-tabu", .extra = extra},
                make<ImprovingTabuSearch>(params));
    }
    for (const size_t cap : {20U, 200U}) {
        auto params = dive;
        params.max_iterations = 1000;
        params.stop_after_X_solutions = cap;
        measure(model,
                dataset,
                {.algorithm = "diversification-greedy",
                 .extra = "stop-after=" + std::to_string(cap) + " iterations=1000"},
                make_diversification(params, nullptr));
    }
}

/// @brief The relaxed-dominance variants: the same subproblem, one resource out of dominance.
///
/// @param model   The subproblem, built with the relaxation, or plain when @p ignored names it.
/// @param dataset Its name.
/// @param ignored Components to leave out of dominance per solve; empty when the model itself is
///                relaxed. Recorded as `ignore=<components>` in the row's `extra`.
inline void sweep_relaxed(PricingModel& model, const std::string& dataset,
                          const std::set<size_t>& ignored = {}) {
    auto base = guarded_params();
    base.dominance_ignored_components = ignored;
    auto bidirectional = bidirectional_params(model.horizon());
    bidirectional.dominance_ignored_components = ignored;
    std::string extra;
    for (const size_t component : ignored) {
        extra += (extra.empty() ? "ignore=" : "+") + std::to_string(component);
    }
    for (const size_t quota : {static_cast<size_t>(MAX_INT), static_cast<size_t>(5)}) {
        auto params = bidirectional;
        params.num_labels_to_extend_by_node = quota;
        measure(model,
                dataset,
                {.algorithm = "bidirectional", .quota = quota, .extra = extra},
                make<Bidirectional::Algo>(params));
        auto forward = base;
        forward.num_labels_to_extend_by_node = quota;
        measure(model,
                dataset,
                {.algorithm = "simple", .quota = quota, .extra = extra},
                make<SimpleDominanceAlgorithm>(forward));
        measure(model,
                dataset,
                {.algorithm = "pushing", .quota = quota, .extra = extra},
                make<PushingDominanceAlgorithm>(forward));
    }
}

inline Instance load_instance(const std::string& name) {
    return bidirectional_benchmark::load(name);
}

/// @brief R101 at full size with the column generation's own duals, early to late.
inline std::vector<Dataset> real_dual_datasets() {
    const std::string root = file_parent_dir(__FILE__, 3);
    std::vector<Dataset> datasets;
    const auto instance = load_instance("R101");
    for (const size_t iteration : {0U, 50U, 100U, 129U}) {
        const std::string name = "R101-iter" + std::to_string(iteration);
        if (!selected(name)) {
            continue;
        }
        datasets.push_back({name,
                            instance,
                            InstanceReader::read_duals(root + "/instances/duals/R101/iter_" +
                                                       std::to_string(iteration) + ".txt")});
    }
    return datasets;
}

/// @brief One 50-customer instance per Solomon family (two for C2), with synthetic duals.
inline std::vector<Dataset> synthetic_datasets() {
    std::vector<Dataset> datasets;
    for (const std::string name :
         {"R101_50", "C101_50", "RC101_50", "C201_50", "RC201_50", "R201_50", "C202_50"}) {
        if (!selected(name)) {
            continue;
        }
        auto instance = load_instance(name);
        auto duals =
            bidirectional_benchmark::synthetic_duals(instance, bidirectional_benchmark::kDualAlpha);
        datasets.push_back({name, std::move(instance), std::move(duals)});
    }
    return datasets;
}

}  // namespace heuristic_sweep

/// @brief E1 on R101 with real column-generation duals, without a route memory.
TEST(HeuristicSweep, DISABLED_E1_RealDuals) {
    namespace hs = heuristic_sweep;
    for (const auto& dataset : hs::real_dual_datasets()) {
        SCOPED_TRACE(dataset.name);
        heuristics::PricingModel model(dataset.instance, dataset.duals, {});
        hs::sweep(model, dataset.name, {});
        heuristics::PricingModel relaxed(dataset.instance,
                                         dataset.duals,
                                         {.relax_load_dominance = true});
        hs::sweep_relaxed(relaxed, dataset.name);
    }
}

/// @brief E1 across the Solomon families at 50 customers, synthetic duals, no route memory.
TEST(HeuristicSweep, DISABLED_E1_Families) {
    namespace hs = heuristic_sweep;
    for (const auto& dataset : hs::synthetic_datasets()) {
        SCOPED_TRACE(dataset.name);
        heuristics::PricingModel model(dataset.instance, dataset.duals, {});
        hs::sweep(model, dataset.name, {});
        heuristics::PricingModel relaxed(dataset.instance,
                                         dataset.duals,
                                         {.relax_load_dominance = true});
        hs::sweep_relaxed(relaxed, dataset.name);
    }
}

namespace heuristic_sweep {

/// @brief Every dataset, real duals first.
inline std::vector<Dataset> all_datasets() {
    auto datasets = real_dual_datasets();
    for (auto& dataset : synthetic_datasets()) {
        datasets.push_back(std::move(dataset));
    }
    return datasets;
}

/// @brief ng-8 with the load, rather than the memory, out of dominance.
inline void sweep_ng_relaxed_load(const Dataset& dataset) {
    const heuristics::ModelOptions options{.relaxation = heuristics::Relaxation::Ng,
                                           .relax_load_dominance = true};
    heuristics::PricingModel relaxed(dataset.instance, dataset.duals, options);
    sweep_relaxed(relaxed, dataset.name);
}

}  // namespace heuristic_sweep

/// @brief E1 under the ng-route relaxation (ng-8), on real and synthetic duals.
TEST(HeuristicSweep, DISABLED_E1_Ng) {
    namespace hs = heuristic_sweep;
    for (const auto& dataset : hs::all_datasets()) {
        SCOPED_TRACE(dataset.name);
        const heuristics::ModelOptions ng{.relaxation = heuristics::Relaxation::Ng};
        heuristics::PricingModel model(dataset.instance, dataset.duals, ng);
        hs::sweep(model, dataset.name, {});
        auto relaxed_options = ng;
        relaxed_options.relax_ng_dominance = true;
        heuristics::PricingModel relaxed(dataset.instance, dataset.duals, relaxed_options);
        hs::sweep_relaxed(relaxed, dataset.name);
        hs::sweep_ng_relaxed_load(dataset);
    }
}

/// @brief Only the ng-8 relaxed-load variant, for a run that already has the rest of E1_Ng.
TEST(HeuristicSweep, DISABLED_E1_NgRelaxedLoad) {
    namespace hs = heuristic_sweep;
    for (const auto& dataset : hs::all_datasets()) {
        SCOPED_TRACE(dataset.name);
        hs::sweep_ng_relaxed_load(dataset);
    }
}

/// @brief Step 1's gate: early stops on the high-pressure datasets, after the scheduler change.
///
/// The exact bidirectional reference, then bidirectional (join on and off) and forward stopped at
/// 5 %, 10 %, 20 %, 35 % and 50 % of its time, without and with an ng-8 memory. Compare with the
/// E1 rows for the same datasets.
TEST(HeuristicSweep, DISABLED_S1_EarlyStops) {
    namespace hs = heuristic_sweep;
    const std::vector<double> fractions{0.05, 0.1, 0.2, 0.35, 0.5};
    for (const auto& dataset : hs::all_datasets()) {
        for (const auto relaxation : {heuristics::Relaxation::None, heuristics::Relaxation::Ng}) {
            const bool high = relaxation == heuristics::Relaxation::None
                                  ? (dataset.name == "RC201_50" || dataset.name == "R201_50" ||
                                     dataset.name == "C202_50")
                                  : (dataset.name == "C201_50" || dataset.name == "RC201_50" ||
                                     dataset.name == "R201_50" || dataset.name == "C202_50");
            if (!high) {
                continue;
            }
            SCOPED_TRACE(dataset.name);
            heuristics::PricingModel model(dataset.instance,
                                           dataset.duals,
                                           {.relaxation = relaxation});
            const auto exact = hs::measure(model,
                                           dataset.name,
                                           {.algorithm = "bidirectional"},
                                           heuristics::make<hs::Bidirectional::Algo>(
                                               heuristics::bidirectional_params(model.horizon())));
            hs::sweep_early_stops(model, dataset.name, exact.seconds, fractions);
        }
    }
}

/// @brief Step 2's gate: E1's relaxed variants again, relaxed per solve on the plain graph.
///
/// Each dataset's plain model runs an exact bidirectional solve, then every relaxed configuration
/// of E1 with `dominance_ignored_components`, then the exact solve again, all on the same graph.
/// The relaxed rows (`extra` = `ignore=<component>`) must match E1's two-graph rows exactly in
/// labels extended and columns, and the two exact solves must agree.
TEST(HeuristicSweep, DISABLED_S1_RelaxedDominance) {
    namespace hs = heuristic_sweep;
    constexpr size_t kLoad = 2;
    constexpr size_t kNgMemory = 3;
    for (const auto& dataset : hs::all_datasets()) {
        SCOPED_TRACE(dataset.name);
        for (const auto relaxation : {heuristics::Relaxation::None, heuristics::Relaxation::Ng}) {
            heuristics::PricingModel model(dataset.instance,
                                           dataset.duals,
                                           {.relaxation = relaxation});
            const auto exact_params = heuristics::bidirectional_params(model.horizon());
            const auto before =
                hs::measure(model,
                            dataset.name,
                            {.algorithm = "bidirectional"},
                            heuristics::make<hs::Bidirectional::Algo>(exact_params));
            hs::sweep_relaxed(model, dataset.name, {kLoad});
            if (relaxation == heuristics::Relaxation::Ng) {
                hs::sweep_relaxed(model, dataset.name, {kNgMemory});
            }
            const auto after =
                heuristics::run(model,
                                dataset.name,
                                {.algorithm = "bidirectional", .extra = "after"},
                                heuristics::make<hs::Bidirectional::Algo>(exact_params));
            EXPECT_EQ(after.extended, before.extended)
                << "the relaxation leaked into a later solve";
            EXPECT_EQ(after.negative_columns, before.negative_columns);
        }
    }
}

/// @brief Step 3's gate: truncated bidirectional labeling, now that a quota sweeps node by node.
///
/// On every dataset, without and with an ng-8 memory: the exact bidirectional reference, then
/// bidirectional at quotas 1 to 50, with a join budget of 200 at quotas 1, 5 and 10, and truncated
/// Pushing at quotas 1, 5 and 10 as the forward reference. Compare the bidirectional rows with
/// E1's, where the quota kept the first labels to arrive at a node rather than the cheapest.
TEST(HeuristicSweep, DISABLED_S1_Truncation) {
    namespace hs = heuristic_sweep;
    for (const auto& dataset : hs::all_datasets()) {
        SCOPED_TRACE(dataset.name);
        for (const auto relaxation : {heuristics::Relaxation::None, heuristics::Relaxation::Ng}) {
            heuristics::PricingModel model(dataset.instance,
                                           dataset.duals,
                                           {.relaxation = relaxation});
            const auto bidirectional = heuristics::bidirectional_params(model.horizon());
            hs::measure(model,
                        dataset.name,
                        {.algorithm = "bidirectional"},
                        heuristics::make<hs::Bidirectional::Algo>(bidirectional));
            for (const size_t quota : {1U, 2U, 5U, 10U, 50U}) {
                auto params = bidirectional;
                params.num_labels_to_extend_by_node = quota;
                hs::measure(model,
                            dataset.name,
                            {.algorithm = "bidirectional", .quota = quota},
                            heuristics::make<hs::Bidirectional::Algo>(params));
            }
            // With a join budget: under a quota the join still pairs every half the bound admits,
            // and recording those columns, not the search, is most of the time.
            for (const size_t quota : {1U, 5U, 10U}) {
                auto params = bidirectional;
                params.num_labels_to_extend_by_node = quota;
                params.join_column_budget = 200;
                hs::measure(model,
                            dataset.name,
                            {.algorithm = "bidirectional", .quota = quota, .join_budget = 200},
                            heuristics::make<hs::Bidirectional::Algo>(params));
            }
            for (const size_t quota : {1U, 5U, 10U}) {
                auto params = heuristics::guarded_params();
                params.num_labels_to_extend_by_node = quota;
                hs::measure(model,
                            dataset.name,
                            {.algorithm = "pushing", .quota = quota},
                            heuristics::make<PushingDominanceAlgorithm>(params));
            }
        }
    }
}
