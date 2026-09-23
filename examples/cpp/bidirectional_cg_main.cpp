// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// What the bidirectional pricer costs over a WHOLE column generation, iteration by iteration.
//
// The benchmarks in `tests/cpp/test_bidirectional_benchmark*.hpp` measure one solve against one
// fixed dual vector. That is the right measurement for a container or a dominance change, and the
// wrong one for anything that learns across solves: a half-way point that adapts, an ng memory
// that grows, a truncated pass that has to feed the next one. None of those can be evaluated one
// solve at a time, and until this file existed nothing here drove the bidirectional algorithm
// through a CG loop at all -- `ng_cg_comparison_main.cpp`, the only other CG driver, runs
// `SimpleDominanceAlgorithm`.
//
// It reports, per pricing solve: status, columns returned, the two direction label counts, the
// dominance-comparison total, joined paths, the H applied, and seconds. `fwd/bwd` is the imbalance
// a moving half-way point is supposed to drive towards 1.
//
// The `H` column doubles as the bound's status: it is `half_way_point_used`, which is 0 exactly
// when `bounded_by_half_way` is false. A late iteration can read 0 here even though every earlier
// one read the real `H`. Such a row is followed by the algorithm's `half_way_off_reason()`, so the
// cause is read rather than guessed. The bound being off is correct but slow.
//
// **The algorithm object persists across the whole CG.** `VRP::solve` reuses the pointers it is
// given rather than constructing one per iteration, so state carried on the algorithm survives
// from one pricing call to the next. That is what a feedback controller over successive solves
// needs, and it is why this driver goes through the `algorithms` vector rather than the variadic
// template parameter -- the latter constructs a fresh object every iteration.
//
// **Three pricers per instance**: forward, bidirectional with a static `H = R/2`, and
// bidirectional with `dynamic_half_way` -- the same persistent object, with a `HalfWayController`
// moving `H` between pricing calls. The `move` column is what the controller did after each solve
// (blank for the static run). Each summary line ends with the imbalance the controller is meant to
// shrink: the geometric mean over bounded iterations of `max(fwd, bwd) / min(fwd, bwd)`, and how
// many iterations sat outside the controller's 20 % dead zone.
//
// Pass instance names to choose them (default `R101_25 R201_25`); `--summary` drops the
// per-iteration tables.
//
// Needs Gurobi, like every other driver here. Works under any `kRouteRelaxation`: all three
// presets declare coherent backward forms, so the bidirectional solve is accepted at setup.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "vrp/instance_reader.hpp"
#include "vrp/vrp.hpp"

namespace {

using ListLC = LabelList<ResourceType>;

/// @brief What one pricing solve cost and produced.
struct PricingRow {
        int iteration = 0;
        AlgorithmStatus status = AlgorithmStatus::COMPLETE;
        size_t solutions = 0;
        size_t forward_labels = 0;
        size_t backward_labels = 0;
        size_t dominance_checks = 0;
        size_t joined_paths = 0;
        bool bounded = false;
        double half_way_point = 0.0;
        double seconds = 0.0;
        /// Why the half-way bound was off; empty when it was in force or the pricer has none.
        std::string off_reason;
        /// What the half-way controller did after this solve; empty when nothing adapts.
        std::string move;
};

/// @brief Wraps an algorithm template and records one row per `solve()`.
///
/// The two-level shape is the one `BidirectionalAlgoBound` uses and for the same reason:
/// `ResourceGraph::create_algorithm` accepts only a `template <typename, typename> class`, so the
/// thing being wrapped has to be bound first and the result has to still be two-parameter.
template <template <typename, typename> class Inner>
struct Recording {
        template <typename RT, typename LC>
        class Algo : public Inner<RT, LC> {
            public:
                using Inner<RT, LC>::Inner;

                SolveResult solve(const Graph<RT>* graph, double cost_upper_bound) override {
                    const auto started = std::chrono::steady_clock::now();
                    SolveResult result = Inner<RT, LC>::solve(graph, cost_upper_bound);
                    const double seconds =
                        std::chrono::duration<double>(std::chrono::steady_clock::now() - started)
                            .count();

                    // Only the bidirectional pricer has a half-way bound to explain.
                    std::string off_reason;
                    if constexpr (requires(const Inner<RT, LC>& algo) {
                                      algo.half_way_off_reason();
                                  }) {
                        off_reason = this->half_way_off_reason();
                    }

                    rows_.push_back(PricingRow{.iteration = static_cast<int>(rows_.size()),
                                               .status = result.status,
                                               .solutions = result.solutions.size(),
                                               .forward_labels = result.forward_labels,
                                               .backward_labels = result.backward_labels,
                                               .dominance_checks = result.dominance_checks,
                                               .joined_paths = result.number_of_joined_paths,
                                               .bounded = result.bounded_by_half_way,
                                               .half_way_point = result.half_way_point_used,
                                               .seconds = seconds,
                                               .off_reason = std::move(off_reason),
                                               .move = {}});
                    // Only the bidirectional algorithm has a controller, and only a dynamic run
                    // seeds it; the forward pricer compiles this branch away.
                    if constexpr (requires(Inner<RT, LC>& algo) { algo.half_way_controller(); }) {
                        if (this->half_way_controller().seeded()) {
                            rows_.back().move = to_string(this->half_way_controller().last_move());
                        }
                    }
                    return result;
                }

                [[nodiscard]] const std::vector<PricingRow>& rows() const { return rows_; }

            private:
                std::vector<PricingRow> rows_;
        };
};

void print_header() {
    std::cout << "    iter  status     cols   fwd_lbl   bwd_lbl  fwd/bwd   dom_checks  joined"
                 "        H      sec  move\n";
}

void print_row(const PricingRow& row) {
    const double ratio = row.backward_labels == 0 ? 0.0
                                                  : static_cast<double>(row.forward_labels) /
                                                        static_cast<double>(row.backward_labels);
    std::cout << "    " << std::setw(4) << std::left << row.iteration << "  " << std::setw(9)
              << to_string(row.status) << std::right << std::setw(6) << row.solutions
              << std::setw(10) << row.forward_labels << std::setw(10) << row.backward_labels
              << std::setw(9) << std::fixed << std::setprecision(2) << ratio << std::setw(13)
              << row.dominance_checks << std::setw(8) << row.joined_paths << std::setw(9)
              << std::setprecision(1) << row.half_way_point << std::setw(9) << std::setprecision(3)
              << row.seconds << "  " << row.move << std::endl;
    if (!row.off_reason.empty()) {
        std::cout << "          bound off: " << row.off_reason << std::endl;
    }
}

/// @brief How far one iteration's split is from balanced: `max(f, b) / min(f, b)`, or 0 when the
///        bound was off or a side is empty, so it cannot be measured.
double imbalance(const PricingRow& row) {
    if (!row.bounded || row.forward_labels == 0 || row.backward_labels == 0) {
        return 0.0;
    }
    const auto forward = static_cast<double>(row.forward_labels);
    const auto backward = static_cast<double>(row.backward_labels);
    return std::max(forward, backward) / std::min(forward, backward);
}

void print_summary(const std::string& label, const CGSolveResult& cg,
                   const std::vector<PricingRow>& rows, double wall_seconds) {
    double pricing_seconds = 0.0;
    size_t checks = 0;
    double log_imbalance = 0.0;
    size_t measured = 0;
    size_t outside_dead_zone = 0;
    for (const auto& row : rows) {
        pricing_seconds += row.seconds;
        checks += row.dominance_checks;
        const double ratio = imbalance(row);
        if (ratio > 0.0) {
            log_imbalance += std::log(ratio);
            ++measured;
            // The controller's own test: |f - b| / min(f, b) > 0.2, i.e. max / min > 1.2.
            if (ratio > 1.2) {  // NOLINT(readability-magic-numbers)
                ++outside_dead_zone;
            }
        }
    }
    std::cout << "    " << label << ": lp_cost=" << std::fixed << std::setprecision(4) << cg.lp_cost
              << "  iterations=" << cg.iterations << "  dom_checks=" << checks
              << "  pricing=" << std::setprecision(2) << pricing_seconds << " s"
              << "  total=" << wall_seconds << " s";
    if (measured > 0) {
        std::cout << "  imbalance=" << std::setprecision(2)
                  << std::exp(log_imbalance / static_cast<double>(measured)) << "x"
                  << "  outside_dead_zone=" << outside_dead_zone << "/" << measured;
    }
    std::cout << (cg.proven_optimal ? "" : "  (NOT proven optimal)") << std::endl;
}

/// @brief One full column generation with the forward pricer.
CGSolveResult run_forward(const Instance& instance) {
    VRP vrp(instance);
    AlgorithmParams<ListLC> params;

    auto algo =
        vrp.get_graph().create_algorithm<Recording<SimpleDominanceAlgorithm>::Algo, ListLC>(params);
    std::vector<Algorithm<ResourceType, ListLC>*> algorithms{algo.get()};

    const auto started = std::chrono::steady_clock::now();
    const auto cg = vrp.solve<>(params, std::nullopt, algorithms);
    const double wall =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

    print_summary("forward      ", cg, algo->rows(), wall);
    return cg;
}

/// @brief One full column generation with the bidirectional pricer, printed per iteration.
///
/// The clock is the time window, which the VRP example registers as the SECOND `RealResource`
/// component -- slot 0 is the cost. `half_way_point` is half the depot's closing time, which is
/// the clock's range; with @p dynamic it is only where the controller starts.
///
/// @param instance      The instance.
/// @param dynamic       Whether `H` adapts between pricing calls.
/// @param per_iteration Whether to print the per-iteration table before the summary.
CGSolveResult run_bidirectional(const Instance& instance, bool dynamic, bool per_iteration) {
    VRP vrp(instance);
    const double horizon = static_cast<double>(instance.get_depot_customer().due_time);

    AlgorithmParams<ListLC> params;
    params.critical_resource_index = 1;  // time
    params.half_way_point = horizon / 2.0;
    params.dynamic_half_way = dynamic;

    auto algo =
        vrp.get_graph()
            .create_algorithm<Recording<BidirectionalAlgoBound<RealResource>::Algo>::Algo, ListLC>(
                params);
    std::vector<Algorithm<ResourceType, ListLC>*> algorithms{algo.get()};

    const auto started = std::chrono::steady_clock::now();
    const auto cg = vrp.solve<>(params, std::nullopt, algorithms);
    const double wall =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

    if (per_iteration) {
        std::cout << "  " << (dynamic ? "dynamic H" : "static H") << "\n";
        print_header();
        for (const auto& row : algo->rows()) {
            print_row(row);
        }
    }
    print_summary(dynamic ? "dynamic H    " : "static H     ", cg, algo->rows(), wall);
    return cg;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> names;
    bool per_iteration = true;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--summary") {
            per_iteration = false;
        } else {
            names.push_back(arg);
        }
    }
    if (names.empty()) {
        names = {"R101_25", "R201_25"};
    }

    std::cout << "[ CG ] forward, static-H and dynamic-H bidirectional, full column generation\n"
              << "       fwd/bwd is the imbalance the dynamic half-way point drives towards 1.\n"
              << std::endl;

    for (const auto& name : names) {
        // Same derivation the sibling drivers use; this file sits at examples/cpp/, three levels
        // below the repository root.
        const std::string root_dir = file_parent_dir(__FILE__, 3);
        InstanceReader reader(root_dir + "/instances/" + name + ".txt");
        const auto instance = reader.read();

        std::cout << "  " << name << std::endl;
        const auto forward = run_forward(instance);
        const auto fixed = run_bidirectional(instance, /*dynamic=*/false, per_iteration);
        const auto adaptive = run_bidirectional(instance, /*dynamic=*/true, per_iteration);

        // The pricers must agree. A difference here is a bug in the algorithm, not in the
        // reporting, and is worth shouting about rather than leaving in a table to be squinted at.
        // Moving H must not change the LP bound either: the half-way bound is correct for any H.
        const std::vector<std::pair<std::string, CGSolveResult>> others{{"static H", fixed},
                                                                        {"dynamic H", adaptive}};
        for (const auto& [label, other] : others) {
            const double gap = std::abs(forward.lp_cost - other.lp_cost);
            if (gap > 1e-6) {  // NOLINT(readability-magic-numbers)
                std::cout << "    *** DISAGREEMENT: forward " << forward.lp_cost << " vs " << label
                          << " " << other.lp_cost << " (gap " << gap << ")" << std::endl;
            }
        }
        std::cout << std::endl;
    }

    return 0;
}
