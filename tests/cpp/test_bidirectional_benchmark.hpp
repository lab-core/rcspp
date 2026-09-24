// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Forward vs. bidirectional benchmarks on VRPTW pricing instances.
//
// Only correctness is asserted; timings and extended-label counts are printed, not checked.
// Extended labels are the machine-independent metric.

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/benchmark_constants.hpp"
#include "vrp/instance_reader.hpp"
#include "vrp_subproblem/vrp_subproblem.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace bidirectional_benchmark {

/// @brief One run's outcome and what it cost to get there.
struct Timed {
        VRPSubproblem::RunMeasurement measurement;
        double seconds = 0.0;
};

/// @brief Runs @p subproblem once and times it.
template <template <typename, typename> class AlgorithmType>
Timed measure(VRPSubproblem* subproblem, const std::map<size_t, double>& duals,
              AlgorithmBaseParams params) {
    const auto started = std::chrono::steady_clock::now();
    Timed timed;
    timed.measurement = subproblem->solve_and_measure<AlgorithmType>(duals, std::move(params));
    timed.seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    return timed;
}

/// @brief Prints one row of the comparison, to stdout so a test run records it.
inline void report(const std::string& name, const Timed& timed) {
    std::cout << "  " << name << ": cost=" << timed.measurement.cost
              << " solutions=" << timed.measurement.solutions
              << " extended_labels=" << timed.measurement.extended_labels
              << " pooled_labels=" << timed.measurement.pooled_labels
              << " status=" << to_string(timed.measurement.status)
              << " bounded=" << timed.measurement.bounded_by_half_way
              << " joined_paths=" << timed.measurement.joined_paths << " seconds=" << timed.seconds
              << std::endl;
}

/// @brief Prints the extended-label ratio, or says why there is not one.
inline void report_ratio(const Timed& forward, const Timed& bidirectional) {
    if (bidirectional.measurement.extended_labels == 0) {
        std::cout << "    ratio: n/a (bidirectional extended no labels)" << std::endl;
        return;
    }
    std::cout << "    ratio (forward / bidirectional extended labels): "
              << static_cast<double>(forward.measurement.extended_labels) /
                     static_cast<double>(bidirectional.measurement.extended_labels)
              << std::endl;
}

/// @brief Synthetic duals for the instances that have no dual files.
///
/// A customer's dual is @p alpha times its round-trip distance from the depot, making distant
/// customers attractive (zero duals would leave nothing to search). These are not real LP duals;
/// they only give both algorithms the same realistic-shaped input.
///
/// @param instance The instance whose customers to price.
/// @param alpha    Scales how attractive visiting a customer is; 1.0 makes a direct out-and-back
///                 trip exactly break even.
inline std::map<size_t, double> synthetic_duals(const Instance& instance, double alpha) {
    const auto& depot = instance.get_depot_customer();
    std::map<size_t, double> duals;
    for (const auto& [customer_id, customer] : instance.get_customers_by_id()) {
        if (customer_id == depot.id) {
            duals[customer_id] = 0.0;
            continue;
        }
        const double dx = customer.pos_x - depot.pos_x;
        const double dy = customer.pos_y - depot.pos_y;
        duals[customer_id] = alpha * 2.0 * std::sqrt((dx * dx) + (dy * dy));
    }
    return duals;
}

/// @brief Reads one Solomon instance by name.
inline Instance load(const std::string& name) {
    const std::string root_dir = file_parent_dir(__FILE__, 3);
    InstanceReader reader(root_dir + "/instances/" + name + ".txt");
    return reader.read();
}

/// @brief Params for a bidirectional run whose clock is the time slot.
inline AlgorithmBaseParams bidirectional_params(double horizon, double timeout_s) {
    AlgorithmBaseParams params;
    params.critical_resource_index = 1;  // time
    params.half_way_point = horizon / 2.0;
    params.timeout_s = timeout_s;
    params.max_memory_gb = kMemoryCapGiB;
    return params;
}

/// @brief Params for a forward reference run.
inline AlgorithmBaseParams forward_params(double timeout_s) {
    AlgorithmBaseParams params;
    params.timeout_s = timeout_s;
    params.max_memory_gb = kMemoryCapGiB;
    return params;
}

}  // namespace bidirectional_benchmark

/// @brief Forward against bidirectional on the VRPTW pricing instance.
///
/// H is half the depot's closing time and the clock is the time slot (1). The bound is asserted
/// to be on, so a silently disabled bound is not mistaken for a measurement of it.
TEST(BidirectionalBenchmark, ForwardVersusBidirectionalOnVrptw) {
    namespace bb = bidirectional_benchmark;

    const std::string instance_name = "R101";
    const std::string root_dir = file_parent_dir(__FILE__, 3);
    InstanceReader reader(root_dir + "/instances/" + instance_name + ".txt");
    const auto instance = reader.read();
    const auto duals =
        InstanceReader::read_duals(root_dir + "/instances/duals/" + instance_name + "/iter_0.txt");

    const double horizon = static_cast<double>(instance.get_depot_customer().due_time);
    ASSERT_GT(horizon, 0.0) << "the depot must close, or there is no middle to aim at";

    VRPSubproblem forward_subproblem(instance);
    const auto forward =
        bb::measure<SimpleDominanceAlgorithm>(&forward_subproblem, duals, AlgorithmBaseParams{});

    AlgorithmBaseParams bidirectional_params;
    bidirectional_params.critical_resource_index = 1;  // time
    bidirectional_params.half_way_point = horizon / 2.0;

    VRPSubproblem bidirectional_subproblem(instance);
    const auto bidirectional =
        bb::measure<BidirectionalAlgoBound<RealResource>::Algo>(&bidirectional_subproblem,
                                                                duals,
                                                                bidirectional_params);

    AlgorithmBaseParams unbounded_params;
    unbounded_params.critical_resource_index = 1;
    unbounded_params.half_way_point = 0.0;  // no explicit H, no finite R: the bound stays off
    VRPSubproblem unbounded_subproblem(instance);
    const auto unbounded =
        bb::measure<BidirectionalAlgoBound<RealResource>::Algo>(&unbounded_subproblem,
                                                                duals,
                                                                unbounded_params);

    std::cout << "[ BENCHMARK ] " << instance_name << ", iteration-0 duals, H = " << horizon / 2.0
              << std::endl;
    bb::report("forward      ", forward);
    bb::report("bidirectional", bidirectional);
    bb::report("bidir (no bound)", unbounded);
    if (bidirectional.measurement.extended_labels > 0) {
        std::cout << "  extended-label ratio (forward / bidirectional): "
                  << static_cast<double>(forward.measurement.extended_labels) /
                         static_cast<double>(bidirectional.measurement.extended_labels)
                  << std::endl;
    }

    // Only correctness is asserted.
    EXPECT_NEAR(forward.measurement.cost, bb::kOptimal, bb::kTolerance)
        << "the forward reference moved; the comparison means nothing until that is explained";
    EXPECT_NEAR(bidirectional.measurement.cost, bb::kOptimal, bb::kTolerance)
        << "bidirectional does not find the known optimum of this instance";
    EXPECT_TRUE(bidirectional.measurement.ref_counts_consistent);
    EXPECT_TRUE(bidirectional.measurement.bounded_by_half_way)
        << "a disabled bound would make this a measurement of two full searches plus a join";
    EXPECT_FALSE(unbounded.measurement.bounded_by_half_way);
    EXPECT_NEAR(unbounded.measurement.cost, bb::kOptimal, bb::kTolerance)
        << "with the bound off the search is exhaustive, so a miss here is not the bound's fault";
}

/// @brief The same instance across column-generation iterations, as the duals grow.
///
/// Subproblems are reused across iterations, as in a column-generation loop, so graph
/// construction is not measured.
TEST(BidirectionalBenchmark, PricingIterationSweep) {
    namespace bb = bidirectional_benchmark;

    const std::string root_dir = file_parent_dir(__FILE__, 3);
    const auto instance = bb::load("R101");
    const double horizon = static_cast<double>(instance.get_depot_customer().due_time);

    VRPSubproblem forward_subproblem(instance);
    VRPSubproblem bidirectional_subproblem(instance);

    std::cout << "[ BENCHMARK ] R101 across CG iterations, H = " << horizon / 2.0 << std::endl;
    for (const size_t iteration : {0U, 25U, 50U, 75U, 100U, 129U}) {
        SCOPED_TRACE("iteration " + std::to_string(iteration));
        const auto duals = InstanceReader::read_duals(root_dir + "/instances/duals/R101/iter_" +
                                                      std::to_string(iteration) + ".txt");

        const auto forward =
            bb::measure<SimpleDominanceAlgorithm>(&forward_subproblem,
                                                  duals,
                                                  bb::forward_params(bb::kTimeoutSeconds));
        const auto bidirectional = bb::measure<BidirectionalAlgoBound<RealResource>::Algo>(
            &bidirectional_subproblem,
            duals,
            bb::bidirectional_params(horizon, bb::kTimeoutSeconds));

        std::cout << "  iter " << iteration << std::endl;
        bb::report("    forward      ", forward);
        bb::report("    bidirectional", bidirectional);
        bb::report_ratio(forward, bidirectional);

        EXPECT_NEAR(bidirectional.measurement.cost, forward.measurement.cost, bb::kTolerance)
            << "the two searches disagree, which makes the comparison meaningless";
        EXPECT_TRUE(bidirectional.measurement.bounded_by_half_way);
    }
}

/// @brief Every Solomon family, run to completion, forward against bidirectional.
///
/// Disabled by default (slow, and a measurement rather than a regression check). Run it with:
///
/// @code tests-rcspp --gtest_also_run_disabled_tests --gtest_filter="*ExactComparison*" @endcode
///
/// Uses truncated instances, since the non-elementary model does not finish at full size, and
/// `synthetic_duals` since these instances ship no dual files.
TEST(BidirectionalBenchmark, DISABLED_ExactComparisonAcrossFamilies) {
    namespace bb = bidirectional_benchmark;

    // All six Solomon families, plus smaller variants to show scaling within a family.
    // Note: C102_50 actually holds 100 customers and RC201_12 holds 20.
    const std::vector<std::string> names{
        // Cheapest family first, so the slowest rows come last.
        //
        // R1: short horizon, narrow windows.
        "R101_25",
        "R101_50",
        "R102_50",
        "R103_50",
        "R105_50",
        "R107_50",
        // C1: clustered, long horizon, narrow windows.
        "C101_25",
        "C101_50",
        "C102_50",
        "C103_50",
        // C2: clustered, the longest horizons in the set.
        "C201_50",
        "C202_50",
        // RC1 and RC2: mixed geography.
        "RC101_50",
        "RC102_50",
        "RC201_12",
        "RC201_50",
        // R2: long horizon, wide windows.
        "R201_25",
        "R201_50",
        "R202_50",
    };

    std::cout << "[ BENCHMARK ] Solomon families at 50 customers, synthetic duals (alpha = "
              << bb::kDualAlpha << ")" << std::endl;
    for (const auto& name : names) {
        SCOPED_TRACE(name);
        const auto instance = bb::load(name);
        const double horizon = static_cast<double>(instance.get_depot_customer().due_time);
        const auto duals = bb::synthetic_duals(instance, bb::kDualAlpha);

        VRPSubproblem forward_subproblem(instance);
        VRPSubproblem bidirectional_subproblem(instance);

        const auto forward =
            bb::measure<SimpleDominanceAlgorithm>(&forward_subproblem,
                                                  duals,
                                                  bb::forward_params(bb::kTimeoutSeconds));
        const auto bidirectional = bb::measure<BidirectionalAlgoBound<RealResource>::Algo>(
            &bidirectional_subproblem,
            duals,
            bb::bidirectional_params(horizon, bb::kTimeoutSeconds));

        std::cout << "  " << name << " (horizon " << horizon << ")" << std::endl;
        bb::report("    forward      ", forward);
        bb::report("    bidirectional", bidirectional);
        bb::report_ratio(forward, bidirectional);

        // Compare answers only when both searches finished; a truncated run would read as a win.
        if (forward.measurement.status == AlgorithmStatus::COMPLETE &&
            bidirectional.measurement.status == AlgorithmStatus::COMPLETE) {
            EXPECT_NEAR(bidirectional.measurement.cost, forward.measurement.cost, bb::kTolerance);
        }
    }
}

/// @brief The same comparison at 75 customers, halfway between the `_50` table and full size.
///
/// Disabled by default, like the test above, and run the same way. Instances are the first 75
/// customers of each Solomon file. The time budget is a runaway guard; rows that time out still
/// print, but skip the cost comparison.
TEST(BidirectionalBenchmark, DISABLED_ExactComparisonAt75Customers) {
    namespace bb = bidirectional_benchmark;

    constexpr double kBudget = 300.0;
    // Cheapest-first, so a long tail does not delay the rest of the table.
    const std::vector<std::string> names{
        // R1 -- short horizon, narrow windows.
        "R101_75",
        "R102_75",
        "R105_75",
        "R103_75",
        "R107_75",
        // C1 -- clustered, long horizon, narrow windows.
        "C101_75",
        "C102_75",
        "C105_75",
        "C103_75",
        // RC1 -- mixed geography, short horizon.
        "RC101_75",
        "RC103_75",
        "RC105_75",
        "RC102_75",
        // C2 -- the longest horizons in the set.
        "C201_75",
        "C203_75",
        "C205_75",
        "C202_75",
        // RC2.
        "RC201_75",
        // R2 -- long horizon, wide windows.
        "R201_75",
    };

    std::cout << "[ BENCHMARK ] Solomon families at 75 customers, synthetic duals (alpha = "
              << bb::kDualAlpha << "), " << kBudget << " s per solve" << std::endl;

    size_t compared = 0;
    for (const auto& name : names) {
        SCOPED_TRACE(name);
        const auto instance = bb::load(name);
        const double horizon = static_cast<double>(instance.get_depot_customer().due_time);
        const auto duals = bb::synthetic_duals(instance, bb::kDualAlpha);

        VRPSubproblem forward_subproblem(instance);
        VRPSubproblem bidirectional_subproblem(instance);

        const auto forward = bb::measure<SimpleDominanceAlgorithm>(&forward_subproblem,
                                                                   duals,
                                                                   bb::forward_params(kBudget));
        const auto bidirectional = bb::measure<BidirectionalAlgoBound<RealResource>::Algo>(
            &bidirectional_subproblem,
            duals,
            bb::bidirectional_params(horizon, kBudget));

        std::cout << "  " << name << " (horizon " << horizon << ")" << std::endl;
        bb::report("    forward      ", forward);
        bb::report("    bidirectional", bidirectional);
        bb::report_ratio(forward, bidirectional);

        // Compare answers only when both searches finished; a truncated run extends fewer labels
        // and would otherwise read as a win.
        if (forward.measurement.status == AlgorithmStatus::COMPLETE &&
            bidirectional.measurement.status == AlgorithmStatus::COMPLETE) {
            EXPECT_NEAR(bidirectional.measurement.cost, forward.measurement.cost, bb::kTolerance);
            ++compared;
        }
        EXPECT_TRUE(bidirectional.measurement.ref_counts_consistent) << name;
    }

    std::cout << "[ SUMMARY ] " << compared << " of " << names.size()
              << " instances completed under both searches and agreed on the optimum" << std::endl;
}

/// @brief The full-size instances: what does each search reach in equal time?
///
/// Disabled by default; run it with `--gtest_also_run_disabled_tests`.
///
/// Full-size instances do not finish, so both searches get the same time budget and report
/// incumbents, as a pricing loop would. Only that both found a solution is asserted.
TEST(BidirectionalBenchmark, DISABLED_LargeInstancesUnderATimeBudget) {
    namespace bb = bidirectional_benchmark;

    // The 100-customer originals, one per family.
    const std::vector<std::string> names{"R101", "C101", "C201", "RC101", "RC201", "R201"};

    std::cout << "[ BENCHMARK ] full-size Solomon instances, synthetic duals (alpha = "
              << bb::kDualAlpha << "), " << bb::kBudgetSeconds << "s budget each" << std::endl;
    for (const auto& name : names) {
        SCOPED_TRACE(name);
        const auto instance = bb::load(name);
        const double horizon = static_cast<double>(instance.get_depot_customer().due_time);
        const auto duals = bb::synthetic_duals(instance, bb::kDualAlpha);

        VRPSubproblem forward_subproblem(instance);
        VRPSubproblem bidirectional_subproblem(instance);

        const auto forward =
            bb::measure<SimpleDominanceAlgorithm>(&forward_subproblem,
                                                  duals,
                                                  bb::forward_params(bb::kBudgetSeconds));
        const auto bidirectional = bb::measure<BidirectionalAlgoBound<RealResource>::Algo>(
            &bidirectional_subproblem,
            duals,
            bb::bidirectional_params(horizon, bb::kBudgetSeconds));

        std::cout << "  " << name << " (horizon " << horizon << ", "
                  << instance.get_customers_by_id().size() << " nodes)" << std::endl;
        bb::report("    forward      ", forward);
        bb::report("    bidirectional", bidirectional);
        bb::report_ratio(forward, bidirectional);

        EXPECT_GT(forward.measurement.solutions, 0U) << "no incumbent to compare";
        EXPECT_GT(bidirectional.measurement.solutions, 0U) << "no incumbent to compare";
        EXPECT_TRUE(bidirectional.measurement.bounded_by_half_way);

        // If both searches finished, they must agree exactly.
        if (forward.measurement.status == AlgorithmStatus::COMPLETE &&
            bidirectional.measurement.status == AlgorithmStatus::COMPLETE) {
            EXPECT_NEAR(bidirectional.measurement.cost, forward.measurement.cost, bb::kTolerance);
        }
    }
}
