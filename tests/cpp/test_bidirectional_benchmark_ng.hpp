// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Bidirectional benchmarks on the ng-path VRPTW model, beside the non-ng ones. This is the model
// whose container component exercises the join's `MergeRule::Custom` set test.
//
// On R101 with iteration-0 duals the ng optimum equals the non-ng `kOptimal`: the optimal column
// is already elementary, so the restriction does not bind there.

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
/// Larger forbids more cycles (tighter, harder); 0 keeps the component but makes it inert, the
/// same-shape control for ng-versus-no-ng comparisons.
constexpr size_t kNgNeighborhoodSize = 8;

/// @brief Ceilings on the R101 extended-label counts, guarding the narrowed ng memory.
///
/// Ceilings rather than exact counts because floating-point tie-breaking in dominance can shift
/// the count slightly across compilers; a reverted memory representation would exceed them.
constexpr size_t kMaxForwardExtensions = 50000;
constexpr size_t kMaxBidirectionalExtensions = 50000;

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
/// Enabled, unlike the other benchmarks here. Asserts that both find the known optimum, that the
/// join runs its set test (`joined_paths > 0`), and that label counts stay under
/// @ref kMaxForwardExtensions. `joined_paths` itself varies across toolchains.
TEST(BidirectionalBenchmarkNg, NgForwardVersusBidirectionalOnVrptw) {
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

    // On this instance the ng optimum equals the non-ng one (see the top of this file).
    EXPECT_NEAR(forward.measurement.cost, bb::kOptimal, bb::kTolerance)
        << "the ng forward reference moved; the comparison means nothing until that is explained";
    EXPECT_NEAR(bidirectional.measurement.cost, bb::kOptimal, bb::kTolerance)
        << "bidirectional does not find the known optimum of the ng model";
    EXPECT_TRUE(bidirectional.measurement.ref_counts_consistent);
    EXPECT_TRUE(bidirectional.measurement.bounded_by_half_way)
        << "a disabled bound would make this a measurement of two full searches plus a join";

    // The join's `MergeRule::Custom` set test must actually run on this model.
    EXPECT_GT(bidirectional.measurement.joined_paths, 0U)
        << "no paths were joined, so the ng component's merge test never ran and this model "
           "measures nothing step 6 changed";

    // Catches a revert of the narrowed ng memory, which would pass every check above.
    EXPECT_LT(forward.measurement.extended_labels, ng::kMaxForwardExtensions)
        << "the forward search extended " << forward.measurement.extended_labels
        << " labels against a ceiling of " << ng::kMaxForwardExtensions
        << ". It was 190 339 before NgPathExtensionFunction stored the memory already narrowed "
           "and 34 475 after, so a number in that region means the narrowing is no longer "
           "reaching InclusionDominanceFunction. Check NgPathExtensionFunction::apply.";
    EXPECT_LT(bidirectional.measurement.extended_labels, ng::kMaxBidirectionalExtensions)
        << "the same regression, seen from the bidirectional side";
}

/// @brief Does ng-path narrowing change what the bidirectional split is worth?
///
/// R101 with the same duals and label shape, ng size 0 (inert) against populated. A measurement,
/// hence `DISABLED_`. Run it with:
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

        // Neither setting changes the optimum on this instance (see the top of this file).
        EXPECT_NEAR(forward.measurement.cost,
                    bidirectional_benchmark::kOptimal,
                    bidirectional_benchmark::kTolerance)
            << "ng_neighborhood_size = " << ng_size;
    }
}

/// @brief The ng model on the shared 75-customer instances: forward against bidirectional.
///
/// Prints labels, status and the forward / bidirectional extended-label ratio per instance
/// (above 1 means the split paid); a row whose status is not `complete` is not comparable.
/// Asserts only that both searches agree wherever both finished. Run it with:
/// @code
/// tests-rcspp --gtest_also_run_disabled_tests --gtest_filter="*NgExactComparisonAt75*"
/// @endcode
TEST(BidirectionalBenchmarkNg, DISABLED_NgExactComparisonAt75Customers) {
    namespace bb = bidirectional_benchmark;
    namespace ng = bidirectional_benchmark_ng;

    constexpr double kBudget = 300.0;

    std::cout << "[ BENCHMARK ] ng(" << ng::kNgNeighborhoodSize
              << ") on the Solomon families at 75 customers, synthetic duals (alpha = "
              << bb::kDualAlpha << "), " << kBudget << " s per solve" << std::endl;

    size_t compared = 0;
    for (const auto& name : bb::seventy_five_customer_instances()) {
        SCOPED_TRACE(name);
        const auto instance = bb::load(name);
        const double horizon = static_cast<double>(instance.get_depot_customer().due_time);
        const auto duals = bb::synthetic_duals(instance, bb::kDualAlpha);

        VRPSubproblemNg forward_subproblem(instance, ng::kNgNeighborhoodSize);
        VRPSubproblemNg bidirectional_subproblem(instance, ng::kNgNeighborhoodSize);

        const auto forward = ng::measure_ng<SimpleDominanceAlgorithm>(&forward_subproblem,
                                                                      duals,
                                                                      bb::forward_params(kBudget));
        const auto bidirectional = ng::measure_ng<BidirectionalAlgoBound<RealResource>::Algo>(
            &bidirectional_subproblem,
            duals,
            bb::bidirectional_params(horizon, kBudget));

        std::cout << "  " << name << " (horizon " << horizon << ")" << std::endl;
        ng::report_ng("    ng forward      ", forward);
        ng::report_ng("    ng bidirectional", bidirectional);
        if (bidirectional.measurement.extended_labels > 0) {
            std::cout << "    ratio (forward / bidirectional extended labels): "
                      << static_cast<double>(forward.measurement.extended_labels) /
                             static_cast<double>(bidirectional.measurement.extended_labels)
                      << std::endl;
        }

        // Compare only where both finished; a truncated run is not a valid comparison.
        if (forward.measurement.status == AlgorithmStatus::COMPLETE &&
            bidirectional.measurement.status == AlgorithmStatus::COMPLETE) {
            EXPECT_NEAR(bidirectional.measurement.cost, forward.measurement.cost, bb::kTolerance)
                << "the two searches disagree on " << name
                << ", which on an ng model means the join and the forward search admit different "
                   "route sets -- see the merge contract in test_equivalence_ng.hpp";
            ++compared;
        }
    }

    std::cout << "[ BENCHMARK ] compared " << compared << " of "
              << bb::seventy_five_customer_instances().size() << " rows" << std::endl;
    EXPECT_GT(compared, 0U)
        << "every row was truncated, so this table compared nothing; raise kBudget";
}

/// @brief What the ng restriction costs, on the shared 75-customer instances.
///
/// Runs {forward, bidirectional} x {ng off, ng on} per instance and prints bidirectional labels
/// on / off and the split ratio on / off. "ng off" is the inert size-0 component, so labels keep
/// the same shape. Costs across ng settings are optima of different models and are not
/// compared; only forward and bidirectional within one setting must agree. Slow; run it with:
/// @code
/// tests-rcspp --gtest_also_run_disabled_tests --gtest_filter="*NgPayoffAt75*"
/// @endcode
TEST(BidirectionalBenchmarkNg, DISABLED_NgPayoffAt75Customers) {
    namespace bb = bidirectional_benchmark;
    namespace ng = bidirectional_benchmark_ng;

    constexpr double kBudget = 300.0;

    std::cout << "[ BENCHMARK ] what ng(" << ng::kNgNeighborhoodSize
              << ") costs, Solomon families at 75 customers, synthetic duals (alpha = "
              << bb::kDualAlpha << "), " << kBudget << " s per solve" << std::endl;

    size_t compared = 0;
    for (const auto& name : bb::seventy_five_customer_instances()) {
        SCOPED_TRACE(name);
        const auto instance = bb::load(name);
        const double horizon = static_cast<double>(instance.get_depot_customer().due_time);
        const auto duals = bb::synthetic_duals(instance, bb::kDualAlpha);

        std::cout << "  " << name << " (horizon " << horizon << ")" << std::endl;

        double ratio_by_setting[2] = {0.0, 0.0};
        size_t bidi_labels_by_setting[2] = {0, 0};
        size_t index = 0;

        for (const size_t ng_size : {size_t{0}, ng::kNgNeighborhoodSize}) {
            VRPSubproblemNg forward_subproblem(instance, ng_size);
            VRPSubproblemNg bidirectional_subproblem(instance, ng_size);

            const auto forward =
                ng::measure_ng<SimpleDominanceAlgorithm>(&forward_subproblem,
                                                         duals,
                                                         bb::forward_params(kBudget));
            const auto bidirectional = ng::measure_ng<BidirectionalAlgoBound<RealResource>::Algo>(
                &bidirectional_subproblem,
                duals,
                bb::bidirectional_params(horizon, kBudget));

            std::cout << "    ng_neighborhood_size = " << ng_size << std::endl;
            ng::report_ng("      forward      ", forward);
            ng::report_ng("      bidirectional", bidirectional);
            if (bidirectional.measurement.extended_labels > 0) {
                ratio_by_setting[index] =
                    static_cast<double>(forward.measurement.extended_labels) /
                    static_cast<double>(bidirectional.measurement.extended_labels);
                std::cout << "      ratio (forward / bidirectional extended labels): "
                          << ratio_by_setting[index] << std::endl;
            }
            bidi_labels_by_setting[index] = bidirectional.measurement.extended_labels;

            // Forward and bidirectional must agree within one ng setting (not across settings).
            if (forward.measurement.status == AlgorithmStatus::COMPLETE &&
                bidirectional.measurement.status == AlgorithmStatus::COMPLETE) {
                EXPECT_NEAR(bidirectional.measurement.cost,
                            forward.measurement.cost,
                            bb::kTolerance)
                    << name << " at ng_neighborhood_size = " << ng_size;
                ++compared;
            }
            ++index;
        }

        // What ng costs the bidirectional solve.
        if (bidi_labels_by_setting[0] > 0 && bidi_labels_by_setting[1] > 0) {
            std::cout << "    ng effect on bidirectional (labels on / off): "
                      << static_cast<double>(bidi_labels_by_setting[1]) /
                             static_cast<double>(bidi_labels_by_setting[0])
                      << std::endl;
        }
        // Whether ng changes what the split is worth; above 1 means the split gains from ng.
        if (ratio_by_setting[0] > 0.0 && ratio_by_setting[1] > 0.0) {
            std::cout << "    ng effect on the split (ratio on / ratio off): "
                      << ratio_by_setting[1] / ratio_by_setting[0] << std::endl;
        }
    }

    std::cout << "[ BENCHMARK ] compared " << compared << " of "
              << 2 * bb::seventy_five_customer_instances().size() << " (instance, ng) cells"
              << std::endl;
    EXPECT_GT(compared, 0U) << "every cell was truncated; raise kBudget";
}
