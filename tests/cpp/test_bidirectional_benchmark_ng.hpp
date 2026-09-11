// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The benchmark's ng-path configuration.
//
// A *second* table, not a replacement. The non-ng model is the baseline every number in
// `analysis/bidirectional-results.md` refers to, so it keeps its harness, its constants and its
// tests untouched; this file adds the elementary-ish variant beside it.
//
// It exists for two reasons, one of which was a question the results document could not answer:
//
//  1. `MergeRule::Custom` (`Disjoint` before step 6) is dispatched **zero** times in every number
//     in those tables -- the model has no container component at all. This is the only model in
//     the repository where the join's set test runs, so it is the only place step 6's cost can be
//     observed.
//  2. `bidirectional-results.md` §3 records the hypothesis that the modest payoff is limited by
//     the model not being elementary, and could not test it. An ng-path relaxation is the
//     standard way to approach elementarity, so this table is that test.
//
// **On the reference optimum.** The plan predicted a *different* constant would be needed here,
// because ng-feasibility is a restriction and a restriction can only make the optimum worse. It
// does not, on this instance: derived from a completed forward search on R101 with the
// iteration-0 duals, the ng optimum is `-319.87786809696524`, i.e. **exactly `kOptimal`**. The
// optimal column is already elementary, so the restriction does not bind at the optimum -- it
// only removes non-elementary columns that were never optimal. Hence no new constant: asserting
// the same `kOptimal` is the stronger statement, and if a future change makes ng bind at the
// optimum this assertion is what will say so.

#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <map>
#include <string>

#include "rcspp/rcspp.hpp"
#include "util/benchmark_constants.hpp"
#include "vrp/instance_reader.hpp"
#include "vrp_subproblem/vrp_subproblem_ng.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace bidirectional_benchmark_ng {

/// @brief How many nearest customers form each node's ng-neighborhood.
///
/// 8 is a conventional ng-route size. Larger forbids more cycles -- a tighter relaxation and a
/// harder subproblem -- and 0 leaves the component present but inert, which is what makes the
/// ng-versus-no-ng rows a controlled comparison rather than a comparison of two differently
/// shaped labels.
constexpr size_t kNgNeighborhoodSize = 8;

/// @brief One ng run's outcome and what it cost.
struct TimedNg {
        VRPSubproblemNg::RunMeasurement measurement;
        double seconds = 0.0;
};

/// @brief Runs @p subproblem once and times it.
///
/// @tparam AlgorithmType The algorithm template to run.
/// @param subproblem The ng subproblem to solve.
/// @param duals      The duals, by customer id.
/// @param params     Algorithm parameters.
/// @return The measurement and the wall clock.
template <template <typename, typename> class AlgorithmType>
TimedNg measure_ng(VRPSubproblemNg* subproblem, const std::map<size_t, double>& duals,
                   AlgorithmBaseParams params) {
    const auto started = std::chrono::steady_clock::now();
    TimedNg timed;
    timed.measurement = subproblem->solve_and_measure<AlgorithmType>(duals, std::move(params));
    timed.seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    return timed;
}

/// @brief Prints one row, to stdout so a test run records it.
inline void report_ng(const std::string& name, const TimedNg& timed) {
    std::cout << "  " << name << ": cost=" << timed.measurement.cost
              << " solutions=" << timed.measurement.solutions
              << " extended_labels=" << timed.measurement.extended_labels
              << " pooled_labels=" << timed.measurement.pooled_labels
              << " status=" << to_string(timed.measurement.status)
              << " bounded=" << timed.measurement.bounded_by_half_way
              << " joined_paths=" << timed.measurement.joined_paths << " seconds=" << timed.seconds
              << std::endl;
}

}  // namespace bidirectional_benchmark_ng

/// @brief The ng model on R101, iteration-0 duals: forward against bidirectional.
///
/// `DISABLED_` like the other heavy benchmarks, and it earns that: **141 s** on this machine,
/// because the ng model extends 8.7x the labels the non-ng one does. R101 at iteration 0 is the
/// easiest pricing problem in the repository and it is still two minutes with a set memory
/// attached. Run it with:
/// @code tests-rcspp --gtest_also_run_disabled_tests --gtest_filter="*NgForwardVersus*" @endcode
///
/// Measured (R101, iteration-0 duals, ng size 8, H = 115):
///
/// | run | cost | extended labels | joined paths | seconds |
/// |---|---|---|---|---|
/// | ng forward       | -319.87786809696524 | 434 398 | -- | 112.4 |
/// | ng bidirectional | -319.87786809696524 | 119 956 | 10 | 28.7 |
///
/// **3.62x fewer extensions and 3.9x less wall clock** -- against roughly 2x on the non-ng model
/// at this instance, which is the first data on `bidirectional-results.md` §3's hypothesis that
/// non-elementarity was limiting the payoff. It supports it.
///
/// What is *asserted* rather than printed: that the ng model still finds the known optimum, and
/// that the join dispatches its set test at all (`joined_paths > 0`), which is what makes step
/// 6's arm reachable.
TEST(BidirectionalBenchmarkNg, DISABLED_NgForwardVersusBidirectionalOnVrptw) {
    namespace bb = bidirectional_benchmark;
    namespace ng = bidirectional_benchmark_ng;

    const std::string instance_name = "R101";
    const std::string root_dir = file_parent_dir(__FILE__, 3);
    InstanceReader reader(root_dir + "/instances/" + instance_name + ".txt");
    const auto instance = reader.read();
    const auto duals =
        InstanceReader::read_duals(root_dir + "/instances/duals/" + instance_name + "/iter_0.txt");

    const double horizon = static_cast<double>(instance.get_depot_customer().due_time);
    ASSERT_GT(horizon, 0.0);

    VRPSubproblemNg forward_subproblem(instance, ng::kNgNeighborhoodSize);
    const auto forward =
        ng::measure_ng<SimpleDominanceAlgorithm>(&forward_subproblem, duals, AlgorithmBaseParams{});

    AlgorithmBaseParams bounded;
    bounded.critical_resource_index = 1;  // time
    bounded.half_way_point = horizon / 2.0;
    VRPSubproblemNg bidirectional_subproblem(instance, ng::kNgNeighborhoodSize);
    const auto bidirectional =
        ng::measure_ng<BidirectionalAlgoBound<RealResource>::Algo>(&bidirectional_subproblem,
                                                                   duals,
                                                                   bounded);

    std::cout << "[ BENCHMARK ] " << instance_name << " + ng(" << ng::kNgNeighborhoodSize
              << "), iteration-0 duals, H = " << horizon / 2.0 << std::endl;
    ng::report_ng("ng forward      ", forward);
    ng::report_ng("ng bidirectional", bidirectional);
    if (bidirectional.measurement.extended_labels > 0) {
        std::cout << "  extended-label ratio (forward / bidirectional): "
                  << static_cast<double>(forward.measurement.extended_labels) /
                         static_cast<double>(bidirectional.measurement.extended_labels)
                  << std::endl;
    }

    // Correctness first. The ng optimum is the same number as the non-ng one on this instance --
    // see the note at the top of this file for why that is a result and not a copy-paste.
    EXPECT_NEAR(forward.measurement.cost, bb::kOptimal, bb::kTolerance)
        << "the ng forward reference moved; the comparison means nothing until that is explained";
    EXPECT_NEAR(bidirectional.measurement.cost, bb::kOptimal, bb::kTolerance)
        << "bidirectional does not find the known optimum of the ng model";
    EXPECT_TRUE(bidirectional.measurement.ref_counts_consistent);
    EXPECT_TRUE(bidirectional.measurement.bounded_by_half_way)
        << "a disabled bound would make this a measurement of two full searches plus a join";

    // The point of the whole file: the join's set test is reached. Without a container component
    // `MergeRule::Custom` is dispatched zero times, and step 6's arm is unobservable.
    EXPECT_GT(bidirectional.measurement.joined_paths, 0U)
        << "no paths were joined, so the ng component's merge test never ran and this model "
           "measures nothing step 6 changed";
}

/// @brief Does ng-path narrowing change what the bidirectional split is worth?
///
/// `bidirectional-results.md` §3 records the hypothesis that the payoff is limited by the model
/// not being elementary. This is the controlled version of that question: the same instance, the
/// same duals and the same three-slot label, with the ng neighborhoods empty in one run and
/// populated in the other, so the only difference is whether ng constrains anything.
///
/// `DISABLED_` because it is four solves rather than two and it is a measurement, not a gate. Run
/// it with:
/// @code tests-rcspp --gtest_also_run_disabled_tests --gtest_filter="*NgPayoff*" @endcode
TEST(BidirectionalBenchmarkNg, DISABLED_NgPayoffComparedToTheInertModel) {
    namespace ng = bidirectional_benchmark_ng;

    const std::string instance_name = "R101";
    const std::string root_dir = file_parent_dir(__FILE__, 3);
    InstanceReader reader(root_dir + "/instances/" + instance_name + ".txt");
    const auto instance = reader.read();
    const auto duals =
        InstanceReader::read_duals(root_dir + "/instances/duals/" + instance_name + "/iter_0.txt");
    const double horizon = static_cast<double>(instance.get_depot_customer().due_time);

    std::cout << "[ BENCHMARK ] " << instance_name << ": does ng change what the split is worth?"
              << std::endl;

    for (const size_t ng_size : {size_t{0}, ng::kNgNeighborhoodSize}) {
        VRPSubproblemNg forward_subproblem(instance, ng_size);
        const auto forward = ng::measure_ng<SimpleDominanceAlgorithm>(&forward_subproblem,
                                                                      duals,
                                                                      AlgorithmBaseParams{});

        AlgorithmBaseParams bounded;
        bounded.critical_resource_index = 1;
        bounded.half_way_point = horizon / 2.0;
        VRPSubproblemNg bidirectional_subproblem(instance, ng_size);
        const auto bidirectional =
            ng::measure_ng<BidirectionalAlgoBound<RealResource>::Algo>(&bidirectional_subproblem,
                                                                       duals,
                                                                       bounded);

        std::cout << "  ng_neighborhood_size = " << ng_size << std::endl;
        ng::report_ng("  forward      ", forward);
        ng::report_ng("  bidirectional", bidirectional);
        if (bidirectional.measurement.extended_labels > 0) {
            std::cout << "    ratio (forward / bidirectional extended labels): "
                      << static_cast<double>(forward.measurement.extended_labels) /
                             static_cast<double>(bidirectional.measurement.extended_labels)
                      << std::endl;
        }

        // An inert ng component must not change the answer; a populated one must not either, on
        // this instance, for the reason at the top of this file.
        EXPECT_NEAR(forward.measurement.cost,
                    bidirectional_benchmark::kOptimal,
                    bidirectional_benchmark::kTolerance)
            << "ng_neighborhood_size = " << ng_size;
    }
}
