// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Compares ng-path neighbourhood sizes over a whole column generation, not a single pricing solve.
// A tighter relaxation should show up as a higher LP bound or fewer iterations. Needs Gurobi.
//
// Usage: rcspp-ng-cg-comparison [--ng-sizes=0,4,8] [--compare-fold] [INSTANCE...]
//
// Per instance and ng size it prints:
//   lp_cost     the LP bound; higher means a tighter relaxation.
//   iterations  how many times the master was re-solved.
//   seconds     total wall clock, pricing plus master.
// ng size 0 keeps the memory registered but inert, so it is a same-shape control.
//
// --compare-fold runs every ng size above 0 a second time with the ng unreachable fold on
// (`VRP::set_ng_unreachable_fold`), and prints both bounds, both times, the folds, and SAME when
// both runs are proven and their bounds agree, DIFFERENT when both are proven and they do not,
// UNPROVEN otherwise. The fold must not change the bound, so the driver exits with status 1 if any
// pair is DIFFERENT.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "vrp/instance_reader.hpp"
#include "vrp/vrp.hpp"

// Without an ng memory every size is the same run, so refuse to build.
static_assert(kRouteRelaxation == RouteRelaxation::NgPath,
              "ng_cg_comparison compares ng sizes, so it needs a build whose kRouteRelaxation is "
              "RouteRelaxation::NgPath. Edit that constant in examples/cpp/vrp/vrp.hpp and "
              "rebuild; under None no route memory is registered and every size is the same run.");

namespace {

/// @brief Parses the comma-separated `--ng-sizes` value. 0 is the inert control.
///
/// Larger neighbourhoods tighten the bound but make pricing harder; the best size depends on
/// the instance.
std::vector<size_t> parse_ng_sizes(const std::string& csv) {
    std::vector<size_t> sizes;
    size_t start = 0;
    while (start <= csv.size()) {
        const size_t comma = csv.find(',', start);
        const std::string token = csv.substr(start, comma - start);
        if (!token.empty()) {
            sizes.push_back(static_cast<size_t>(std::stoul(token)));
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return sizes;
}

/// @brief One full column generation, timed.
struct CGRun {
        double lp_cost = 0.0;
        int iterations = 0;
        double seconds = 0.0;
        bool proven_optimal = true;
        size_t unreachable_folds = 0;
};

/// @brief Per-pricing-solve budget, so a hard configuration reports instead of hanging.
///
/// A truncated solve still returns improving columns. Only if the final pricing solve is cut
/// short is the bound unproven, which `CGSolveResult::proven_optimal` reports.
constexpr double kPricingBudgetSeconds = 120.0;

/// @brief Runs one full column generation on @p instance with ng size @p ng_size, and times it.
///
/// @param fold Whether pricing folds unreachable ng-neighbours into the labels' memories.
CGRun run_cg(const Instance& instance, size_t ng_size, bool fold = false) {
    VRP vrp(instance, ng_size);
    vrp.set_ng_unreachable_fold(fold);
    AlgorithmParams<LabelList<ResourceType>> params;
    params.timeout_s = kPricingBudgetSeconds;

    const auto started = std::chrono::steady_clock::now();
    const auto result = vrp.solve<SimpleDominanceAlgorithm>(params);
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

    return {result.lp_cost,
            result.iterations,
            seconds,
            result.proven_optimal,
            result.unreachable_folds};
}

/// @brief Whether two proven bounds agree, within a relative tolerance.
bool same_bound(double lhs, double rhs) {
    constexpr double kRelativeTolerance = 1e-6;
    return std::abs(lhs - rhs) <= kRelativeTolerance * std::max(1.0, std::abs(lhs));
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> names;
    std::vector<size_t> sizes{0, 8};
    bool compare_fold = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.starts_with("--ng-sizes=")) {
            sizes = parse_ng_sizes(arg.substr(std::string("--ng-sizes=").size()));
        } else if (arg == "--compare-fold") {
            compare_fold = true;
        } else {
            names.push_back(arg);
        }
    }
    if (names.empty()) {
        // A quick default mix: short horizon, clustered long horizon, and two scattered long
        // horizons.
        names = {"R101_25", "C201_50", "R201_25", "RC201_50"};
    }

    std::cout << "[ CG ] ng off (inert) against ng on, full column generation\n"
              << "       lp_cost is the bound -- higher is tighter, and the point of ng.\n"
              << std::endl;

    bool any_different = false;

    for (const auto& name : names) {
        // This file is three levels below the repository root.
        const std::string root_dir = file_parent_dir(__FILE__, 3);
        InstanceReader reader(root_dir + "/instances/" + name + ".txt");
        const auto instance = reader.read();

        std::cout << "  " << name << std::endl;
        CGRun baseline;
        bool have_baseline = false;

        for (const size_t ng_size : sizes) {
            const CGRun run = run_cg(instance, ng_size);
            std::cout << "    ng=" << ng_size << std::setw(4) << "" << "lp_cost=" << std::fixed
                      << std::setprecision(4) << run.lp_cost << "  iterations=" << run.iterations
                      << "  seconds=" << std::setprecision(2) << run.seconds
                      << (run.proven_optimal ? "" : "  (NOT proven optimal)") << std::endl;

            if (compare_fold && ng_size > 0) {
                const CGRun folded = run_cg(instance, ng_size, /*fold=*/true);
                const bool proven = run.proven_optimal && folded.proven_optimal;
                const bool same = same_bound(run.lp_cost, folded.lp_cost);
                any_different = any_different || (proven && !same);
                std::cout << "      fold: lp_cost=" << std::setprecision(4) << folded.lp_cost
                          << "  iterations=" << folded.iterations
                          << "  seconds=" << std::setprecision(2) << folded.seconds << " (x"
                          << folded.seconds / std::max(run.seconds, 1e-9)
                          << ")  folds=" << folded.unreachable_folds << "  "
                          << (!proven ? "UNPROVEN" : (same ? "SAME" : "DIFFERENT")) << std::endl;
            }

            if (!have_baseline) {
                baseline = run;
                have_baseline = true;
                continue;
            }
            // Change relative to the first size (usually the inert ng=0 control).
            std::cout << "      bound delta: " << std::showpos << std::setprecision(4)
                      << run.lp_cost - baseline.lp_cost << std::noshowpos
                      << "   iterations: " << baseline.iterations << " -> " << run.iterations
                      << "   wall clock: " << std::setprecision(2) << baseline.seconds << " s -> "
                      << run.seconds << " s" << std::endl;
        }
    }

    return any_different ? 1 : 0;
}
