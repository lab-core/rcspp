// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The join-optimality sweep's instances and solves, shared by the join-optimality tests and the
// tests of every strategy that searches bidirectionally.

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/bidirectional_test_util.hpp"
#include "util/random_instance.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace join_optimality {

using Composed = ResourceTypeComposition<RealResource>;

constexpr double kTolerance = 1e-9;

/// @brief One solve's outcome.
struct Outcome {
        double cost = std::numeric_limits<double>::infinity();
        size_t solutions = 0;
        size_t joined_paths = 0;
        bool bounded = false;
};

/// @brief Solves one instance bidirectionally with @p Strategy, with `H` at a given fraction of
///        the clock's range.
///
/// @tparam Strategy The labelling strategy that searches bidirectionally (Simple by default).
/// @param config   The instance to build.
/// @param h_factor `H` as a fraction of the clock's upper bound; 0 leaves the bound disabled.
/// @return What the solve returned.
template <typename Strategy = strategy::Simple>
Outcome solve_with(const test_util::InstanceConfig& config, double h_factor) {
    auto built = test_util::build_instance(config);

    AlgorithmParams<LabelList<Composed>> params;
    params.critical_resource_index = built.clock_index;
    params.half_way_point = built.clock_upper_bound * h_factor;

    auto algorithm =
        built.graph->template create_algorithm<Strategy>(test_util::bidirectional(params));
    const auto result = built.graph->solve(algorithm.get());

    Outcome outcome;
    outcome.cost = result.solutions.empty() ? std::numeric_limits<double>::infinity()
                                            : result.solutions.front().cost;
    outcome.solutions = result.solutions.size();
    outcome.joined_paths = result.bidirectional->joined_paths;
    outcome.bounded = result.bidirectional->half_way_used;
    return outcome;
}

/// @brief The forward reference, for instances too large to enumerate.
inline double forward_optimum(const test_util::InstanceConfig& config) {
    auto built = test_util::build_instance(config);
    const auto result = built.graph->solve<strategy::Simple>(AlgorithmBaseParams{});
    return result.solutions.empty() ? std::numeric_limits<double>::infinity()
                                    : result.solutions.front().cost;
}

/// @brief Every combination of the generator's knobs, at @p num_seeds seeds each.
///
/// @param num_seeds How many seeds per combination.
/// @param num_nodes Instance size. Small, because `brute_force_optimum` is exponential.
/// @return The configurations.
inline std::vector<test_util::InstanceConfig> cross_product(std::uint32_t num_seeds,
                                                            size_t num_nodes) {
    std::vector<test_util::InstanceConfig> configs;
    for (std::uint32_t seed = 0; seed < num_seeds; ++seed) {
        for (const bool with_time_window : {false, true}) {
            for (const bool with_capacity : {false, true}) {
                for (const bool mixed_sign_costs : {false, true}) {
                    for (const size_t num_sinks : {size_t{1}, size_t{2}}) {
                        for (const double density : {0.3, 0.75}) {
                            test_util::InstanceConfig config;
                            config.num_nodes = num_nodes;
                            config.density = density;
                            config.seed = seed;
                            config.with_time_window = with_time_window;
                            config.window_slack = with_time_window ? 0.6 : 1.0;
                            config.with_capacity = with_capacity;
                            config.capacity_binds = with_capacity;
                            config.mixed_sign_costs = mixed_sign_costs;
                            config.num_sinks = num_sinks;
                            configs.push_back(config);
                        }
                    }
                }
            }
        }
    }
    return configs;
}

/// @brief Where to put `H`, as a fraction of the clock's range.
///
/// Low values leave most paths to the join, high values to the forward search; 0 disables the
/// bound.
inline const std::vector<double>& half_way_factors() {
    static const std::vector<double> factors{0.0, 0.25, 0.5, 0.75};
    return factors;
}

}  // namespace join_optimality
