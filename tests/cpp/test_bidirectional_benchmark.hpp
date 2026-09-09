// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Phase 13 section 6: the payoff measurement, on the repository's own VRPTW pricing instance.
//
// This runs LAST, and deliberately so. **A policy that loses solutions looks fast.** Measured
// first, a large speedup says nothing about whether the work or the answers were pruned -- which
// is why the equivalence sweep and the oracle come before this file, and why the first assertion
// here is that both algorithms find the known optimum.
//
// The numbers are printed rather than asserted. A wall-clock threshold in a unit test is a flaky
// test; the point of the measurement is the record, not a gate. Label *extensions* are the metric
// that carries across machines -- the same instance on a busier box takes longer without any
// algorithm having changed.
//
// The instance is R101 with the iteration-0 duals, the same one the existing forward tests pin to
// an optimum of -319.87786809696524415, so most arcs carry a negative reduced cost: the realistic
// pricing case rather than a shortest-path toy.
//
// The measured numbers and what they mean are written up in `analysis/bidirectional-results.md`.

#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <map>
#include <string>

#include "rcspp/rcspp.hpp"
#include "vrp/instance_reader.hpp"
#include "vrp_subproblem/vrp_subproblem.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace bidirectional_benchmark {

constexpr double kTolerance = 1e-9;
constexpr double kOptimal = -319.87786809696524415;

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
              << " bounded=" << timed.measurement.bounded_by_half_way
              << " joined_paths=" << timed.measurement.joined_paths << " seconds=" << timed.seconds
              << std::endl;
}

}  // namespace bidirectional_benchmark

/// @brief Forward against bidirectional on the VRPTW pricing instance.
///
/// The half-way point is the depot's closing time halved -- the natural middle of the routing
/// horizon -- and the clock is the time component, slot 1 within the `RealResource` type. Pointing
/// it at slot 0 would name the reduced cost, which is not monotone, and the bound would switch
/// itself off; `bounded_by_half_way()` is asserted so a silently disabled bound cannot be reported
/// as a measurement of the bound.
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

    // Correctness first, and the only thing asserted.
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
