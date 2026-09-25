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
// **The join's budgets** (PR 3), for measuring what bounding the join's work buys:
//   --join-budget=K       join_column_budget: the join keeps only its K cheapest paths
//   --max-join-pairs=N    max_join_pairs: the join stops after N merge-rule questions
//   --pricing-timeout=S   timeout_s on every pricing solve, forward included
//   --no-join-after-stop  join_after_early_stop = false: skip the join after a timeout, as before
//   --no-forward          skip the forward pricer; the dynamic run is then checked against static
// The `pairs` column is `join_pairs_tested`; a `J` after the move means the join was truncated.
//
// Needs Gurobi, like every other driver here. Works under any `kRouteRelaxation`: all three
// presets declare coherent backward forms, so the bidirectional solve is accepted at setup.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
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
        size_t join_pairs = 0;
        bool join_truncated = false;
};

/// @brief What a run caps. Every default is "no cap", which reproduces PR 2's driver.
struct JoinOptions {
        size_t join_budget = MAX_INT;                                ///< --join-budget=K
        size_t max_pairs = MAX_INT;                                  ///< --max-join-pairs=N
        double timeout_s = std::numeric_limits<double>::infinity();  ///< --pricing-timeout=S
        bool join_after_stop = true;                                 ///< --no-join-after-stop
        bool run_forward = true;                                     ///< --no-forward
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
                                               .move = {},
                                               .join_pairs = result.join_pairs_tested,
                                               .join_truncated = result.join_truncated});
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
                 "       pairs        H      sec  move\n";
}

void print_row(const PricingRow& row) {
    const double ratio = row.backward_labels == 0 ? 0.0
                                                  : static_cast<double>(row.forward_labels) /
                                                        static_cast<double>(row.backward_labels);
    std::cout << "    " << std::setw(4) << std::left << row.iteration << "  " << std::setw(9)
              << to_string(row.status) << std::right << std::setw(6) << row.solutions
              << std::setw(10) << row.forward_labels << std::setw(10) << row.backward_labels
              << std::setw(9) << std::fixed << std::setprecision(2) << ratio << std::setw(13)
              << row.dominance_checks << std::setw(8) << row.joined_paths << std::setw(12)
              << row.join_pairs << std::setw(9) << std::setprecision(1) << row.half_way_point
              << std::setw(9) << std::setprecision(3) << row.seconds << "  " << row.move
              << (row.join_truncated ? " J" : "") << std::endl;
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
    size_t columns = 0;
    size_t timeouts = 0;
    size_t truncated_joins = 0;
    for (const auto& row : rows) {
        pricing_seconds += row.seconds;
        checks += row.dominance_checks;
        columns += row.solutions;
        timeouts += row.status == AlgorithmStatus::TIMEOUT ? 1 : 0;
        truncated_joins += row.join_truncated ? 1 : 0;
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
              << "  total=" << wall_seconds << " s" << "  cols=" << columns
              << "  timeouts=" << timeouts << "  jtrunc=" << truncated_joins;
    if (measured > 0) {
        std::cout << "  imbalance=" << std::setprecision(2)
                  << std::exp(log_imbalance / static_cast<double>(measured)) << "x"
                  << "  outside_dead_zone=" << outside_dead_zone << "/" << measured;
    }
    std::cout << (cg.proven_optimal ? "" : "  (NOT proven optimal)") << std::endl;
}

/// @brief One full column generation with the forward pricer.
///
/// Only the pricing timeout applies: the join budgets have nothing to cap in a forward search.
CGSolveResult run_forward(const Instance& instance, const JoinOptions& options) {
    VRP vrp(instance);
    AlgorithmParams<ListLC> params;
    params.timeout_s = options.timeout_s;

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
/// @param options       The join's budgets and the pricing timeout.
CGSolveResult run_bidirectional(const Instance& instance, bool dynamic, bool per_iteration,
                                const JoinOptions& options) {
    VRP vrp(instance);
    const double horizon = static_cast<double>(instance.get_depot_customer().due_time);

    AlgorithmParams<ListLC> params;
    params.critical_resource_index = 1;  // time
    params.half_way_point = horizon / 2.0;
    params.dynamic_half_way = dynamic;
    params.join_column_budget = options.join_budget;
    params.max_join_pairs = options.max_pairs;
    params.timeout_s = options.timeout_s;
    params.join_after_early_stop = options.join_after_stop;

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
    JoinOptions options;
    // The value after `=` in an option spelled `--name=value`.
    const auto value_of = [](const std::string& arg) { return arg.substr(arg.find('=') + 1); };
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--summary") {
            per_iteration = false;
        } else if (arg.starts_with("--join-budget=")) {
            options.join_budget = std::stoull(value_of(arg));
        } else if (arg.starts_with("--max-join-pairs=")) {
            options.max_pairs = std::stoull(value_of(arg));
        } else if (arg.starts_with("--pricing-timeout=")) {
            options.timeout_s = std::stod(value_of(arg));
        } else if (arg == "--no-join-after-stop") {
            options.join_after_stop = false;
        } else if (arg == "--no-forward") {
            options.run_forward = false;
        } else {
            names.push_back(arg);
        }
    }
    if (names.empty()) {
        names = {"R101_25", "R201_25"};
    }

    std::cout << "[ CG ] forward, static-H and dynamic-H bidirectional, full column generation\n"
              << "       fwd/bwd is the imbalance the dynamic half-way point drives towards 1.\n";
    // Every log states what it was run with.
    const auto limit = [](size_t value) {
        return value >= MAX_INT ? std::string("none") : std::to_string(value);
    };
    std::cout << "       join_budget=" << limit(options.join_budget)
              << "  max_join_pairs=" << limit(options.max_pairs) << "  pricing_timeout="
              << (std::isfinite(options.timeout_s) ? std::to_string(options.timeout_s) + " s"
                                                   : std::string("none"))
              << "  join_after_early_stop=" << (options.join_after_stop ? "yes" : "no")
              << "  forward=" << (options.run_forward ? "yes" : "no") << "\n"
              << std::endl;

    for (const auto& name : names) {
        // Same derivation the sibling drivers use; this file sits at examples/cpp/, three levels
        // below the repository root.
        const std::string root_dir = file_parent_dir(__FILE__, 3);
        InstanceReader reader(root_dir + "/instances/" + name + ".txt");
        const auto instance = reader.read();

        std::cout << "  " << name << std::endl;
        std::optional<CGSolveResult> forward;
        if (options.run_forward) {
            forward = run_forward(instance, options);
        }
        const auto fixed = run_bidirectional(instance, /*dynamic=*/false, per_iteration, options);
        const auto adaptive = run_bidirectional(instance, /*dynamic=*/true, per_iteration, options);

        // The pricers must agree. A difference here is a bug in the algorithm, not in the
        // reporting, and is worth shouting about rather than leaving in a table to be squinted at.
        // Moving H must not change the LP bound either: the half-way bound is correct for any H.
        // Without the forward run, the static run is the reference. A CG that ended unproven
        // (a pricing timeout) may legitimately stop at a different LP value, so it is flagged
        // rather than called a disagreement.
        const std::pair<std::string, CGSolveResult> reference =
            forward ? std::pair{std::string("forward"), *forward}
                    : std::pair{std::string("static H"), fixed};
        std::vector<std::pair<std::string, CGSolveResult>> others{{"dynamic H", adaptive}};
        if (forward) {
            others.insert(others.begin(), {"static H", fixed});
        }
        for (const auto& [label, other] : others) {
            const double gap = std::abs(reference.second.lp_cost - other.lp_cost);
            if (gap > 1e-6) {  // NOLINT(readability-magic-numbers)
                const bool proven = reference.second.proven_optimal && other.proven_optimal;
                std::cout << "    " << (proven ? "*** DISAGREEMENT" : "(unproven) difference")
                          << ": " << reference.first << " " << reference.second.lp_cost << " vs "
                          << label << " " << other.lp_cost << " (gap " << gap << ")" << std::endl;
            }
        }
        std::cout << std::endl;
    }

    return 0;
}
