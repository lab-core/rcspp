// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Pricing times of every labelling strategy and direction, printed when RCSPP_TIMING is set, for
// comparing two builds of the library. Its own translation unit: it instantiates every search on
// the VRP subproblem's resources.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/benchmark_constants.hpp"
#include "vrp/instance_reader.hpp"
#include "vrp_subproblem/vrp_subproblem.hpp"

namespace kernel_timing {

namespace bb = bidirectional_benchmark;

/// @brief A pricing graph: its name in a TIMING line, the instance, the duals and the customers.
struct Pricing {
        std::string name;
        Instance instance;
        std::map<size_t, double> duals;
        size_t customers = 0;
};

/// @brief Solves @p pricing with @p Strategy in @p direction, and prints
///        `TIMING <kind>@<pricing> <customers> <microseconds>`.
template <template <typename, typename> class Strategy>
void time_one(const std::string& kind, const Pricing& pricing, rcspp::SearchDirection direction) {
    constexpr double kTimeoutSeconds = 600.0;
    const double horizon = static_cast<double>(pricing.instance.get_depot_customer().due_time);
    rcspp::AlgorithmBaseParams params = direction == rcspp::SearchDirection::Bidirectional
                                            ? bb::bidirectional_params(horizon, kTimeoutSeconds)
                                            : bb::forward_params(kTimeoutSeconds);
    params.direction = direction;
    VRPSubproblem subproblem(pricing.instance);
    const auto start = std::chrono::steady_clock::now();
    const auto measurement = subproblem.solve_and_measure<Strategy>(pricing.duals, params);
    const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::steady_clock::now() - start)
                                  .count();
    EXPECT_EQ(measurement.status, rcspp::AlgorithmStatus::Complete) << kind << "@" << pricing.name;
    std::printf("TIMING %s@%s %zu %lld\n",
                kind.c_str(),
                pricing.name.c_str(),
                pricing.customers,
                static_cast<long long>(microseconds));
}

/// @brief R101 at five column-generation iterations, with its recorded duals; R103_50 and C201_50
///        with synthetic duals.
///
/// Every solve takes well under 30 s with asserts off. RC201_50 is left out: there, A* and the
/// backward search take over 30 s each.
inline std::vector<Pricing> pricing_graphs() {
    const std::string root = file_parent_dir(__FILE__, 3);
    std::vector<Pricing> all;
    const Instance r101 = bb::load("R101");
    for (const int iteration : {0, 30, 60, 90, 120}) {
        all.push_back({"R101:" + std::to_string(iteration),
                       r101,
                       InstanceReader::read_duals(root + "/instances/duals/R101/iter_" +
                                                  std::to_string(iteration) + ".txt"),
                       100});
    }
    for (const char* name : {"R103_50", "C201_50"}) {
        const Instance instance = bb::load(name);
        all.push_back({name, instance, bb::synthetic_duals(instance, bb::kDualAlpha), 50});
    }
    return all;
}

}  // namespace kernel_timing

/// @brief With RCSPP_TIMING set, prints the pricing time of every strategy and direction on every
///        pricing graph; skipped otherwise.
TEST(KernelTiming, SolomonPricingIsPrintedWhenAsked) {
    namespace kt = kernel_timing;
    if (std::getenv("RCSPP_TIMING") == nullptr) {
        GTEST_SKIP();
    }
    using rcspp::SearchDirection;
    for (const auto& pricing : kt::pricing_graphs()) {
        kt::time_one<rcspp::SimpleDominanceAlgorithm>("simple_forward",
                                                      pricing,
                                                      SearchDirection::Forward);
        kt::time_one<rcspp::SimpleDominanceAlgorithm>("simple_backward",
                                                      pricing,
                                                      SearchDirection::Backward);
        kt::time_one<rcspp::SimpleDominanceAlgorithm>("simple_bidirectional",
                                                      pricing,
                                                      SearchDirection::Bidirectional);
        kt::time_one<rcspp::AStarDominanceAlgorithm>("astar_forward",
                                                     pricing,
                                                     SearchDirection::Forward);
        kt::time_one<rcspp::PushingDominanceAlgorithm>("pushing_forward",
                                                       pricing,
                                                       SearchDirection::Forward);
        kt::time_one<rcspp::PullingDominanceAlgorithm>("pulling_forward",
                                                       pricing,
                                                       SearchDirection::Forward);
    }
}
