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
// one read the real `H` -- once the upper bound prunes the graph hard enough there may be no arc
// left for the clock's monotonicity probe to read, and the bound then switches itself off. Correct
// but slow, on a subproblem that by then has almost nothing in it.
//
// **The algorithm object persists across the whole CG.** `VRP::solve` reuses the pointers it is
// given rather than constructing one per iteration, so state carried on the algorithm survives
// from one pricing call to the next. That is what a feedback controller over successive solves
// needs, and it is why this driver goes through the `algorithms` vector rather than the variadic
// template parameter -- the latter constructs a fresh object every iteration.
//
// Needs Gurobi, like every other driver here. Works under any `kRouteRelaxation`: all three
// presets declare coherent backward forms, so the bidirectional solve is accepted at setup.

#include <chrono>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
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

                    rows_.push_back(PricingRow{.iteration = static_cast<int>(rows_.size()),
                                               .status = result.status,
                                               .solutions = result.solutions.size(),
                                               .forward_labels = result.forward_labels,
                                               .backward_labels = result.backward_labels,
                                               .dominance_checks = result.dominance_checks,
                                               .joined_paths = result.number_of_joined_paths,
                                               .bounded = result.bounded_by_half_way,
                                               .half_way_point = result.half_way_point_used,
                                               .seconds = seconds});
                    return result;
                }

                [[nodiscard]] const std::vector<PricingRow>& rows() const { return rows_; }

            private:
                std::vector<PricingRow> rows_;
        };
};

void print_header() {
    std::cout << "    iter  status     cols   fwd_lbl   bwd_lbl  fwd/bwd   dom_checks  joined"
                 "        H      sec\n";
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
              << row.seconds << std::endl;
}

void print_summary(const std::string& label, const CGSolveResult& cg,
                   const std::vector<PricingRow>& rows, double wall_seconds) {
    double pricing_seconds = 0.0;
    size_t checks = 0;
    for (const auto& row : rows) {
        pricing_seconds += row.seconds;
        checks += row.dominance_checks;
    }
    std::cout << "    " << label << ": lp_cost=" << std::fixed << std::setprecision(4) << cg.lp_cost
              << "  iterations=" << cg.iterations << "  dom_checks=" << checks
              << "  pricing=" << std::setprecision(2) << pricing_seconds << " s"
              << "  total=" << wall_seconds << " s"
              << (cg.proven_optimal ? "" : "  (NOT proven optimal)") << std::endl;
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
/// the clock's range.
CGSolveResult run_bidirectional(const Instance& instance) {
    VRP vrp(instance);
    const double horizon = static_cast<double>(instance.get_depot_customer().due_time);

    AlgorithmParams<ListLC> params;
    params.critical_resource_index = 1;  // time
    params.half_way_point = horizon / 2.0;

    auto algo =
        vrp.get_graph()
            .create_algorithm<Recording<BidirectionalAlgoBound<RealResource>::Algo>::Algo, ListLC>(
                params);
    std::vector<Algorithm<ResourceType, ListLC>*> algorithms{algo.get()};

    const auto started = std::chrono::steady_clock::now();
    const auto cg = vrp.solve<>(params, std::nullopt, algorithms);
    const double wall =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

    print_header();
    for (const auto& row : algo->rows()) {
        print_row(row);
    }
    print_summary("bidirectional", cg, algo->rows(), wall);
    return cg;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> names;
    for (int i = 1; i < argc; ++i) {
        names.emplace_back(argv[i]);
    }
    if (names.empty()) {
        names = {"R101_25", "R201_25"};
    }

    std::cout << "[ CG ] forward against bidirectional, full column generation\n"
              << "       fwd/bwd is the imbalance a moving half-way point would drive towards 1.\n"
              << std::endl;

    for (const auto& name : names) {
        // Same derivation the sibling drivers use; this file sits at examples/cpp/, three levels
        // below the repository root.
        const std::string root_dir = file_parent_dir(__FILE__, 3);
        InstanceReader reader(root_dir + "/instances/" + name + ".txt");
        const auto instance = reader.read();

        std::cout << "  " << name << std::endl;
        const auto forward = run_forward(instance);
        const auto bidirectional = run_bidirectional(instance);

        // The two pricers must agree. A difference here is a bug in the algorithm, not in the
        // reporting, and is worth shouting about rather than leaving in a table to be squinted at.
        const double gap = std::abs(forward.lp_cost - bidirectional.lp_cost);
        if (gap > 1e-6) {
            std::cout << "    *** DISAGREEMENT: forward " << forward.lp_cost << " vs "
                      << "bidirectional " << bidirectional.lp_cost << " (gap " << gap << ")"
                      << std::endl;
        }
        std::cout << std::endl;
    }

    return 0;
}
