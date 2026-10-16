// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Dynamic ng augmentation against fixed ng sizes, over whole column generations.
//
// The ng CG study (ng_cg_comparison_main.cpp) found the useful neighbourhood size is a
// property of the instance: R201_25 is done at ng(4), RC201_50 needs ng(12), and overshooting has
// no cost ceiling. This driver grows the neighbourhoods instead, RouteOpt's way: run column
// generation to a proven optimum, find the cycles in the LP solution's columns, add each repeated
// node to the neighbourhoods of the nodes inside its cycle (examples/cpp/vrp/ng_growth.hpp), remove
// the master columns that made infeasible, and resume. It stops when the bound tails off, when
// pricing gets too slow against the first round (a soft stop, and a hard one that rolls the last
// growth back), when there is nothing left to add, or after a round cap.
//
// The time brakes compare the mean seconds per pricing solve of a round with round 1's: RouteOpt's
// "a pricing pass exceeds". A round *total* would depend on how many iterations the round ran, and
// under a pricing budget, where every solve is short, a round total at the noise level tripped the
// 1x soft brake: R202_25 stopped at 409.36 instead of 411.49. Untimed, round 1 is 87-99 % of all
// pricing and the brakes stay dormant either way; they are a safety net for larger neighbourhood
// caps, not a tuned control.
//
// --reduce shrinks the start table to the LP solution's support after round 1. That lowers the
// bound by design, so the round after it is a new baseline: the no-improvement and tail-off tests
// skip it, and later rounds are measured from its bound.
//
// Usage: rcspp-ng-dynamic-cg [--fixed=0,4,8,12] [--no-dynamic] [--start=N] [--reduce]
//                           [--pricer=forward|bidirectional] [--pricing-timeout=S]
//                           [--pricing-budget=S] [--budget-reset] [--verify]
//                           [--dynamic-h] [--join-budget=K] [--max-join-pairs=N] [INSTANCE...]
//
// With --pricer=bidirectional, --dynamic-h lets the half-way point adapt between pricing solves
// (dynamic_half_way; the persistent pricer carries the controller across ng swaps too),
// --join-budget=K keeps the join's K cheapest paths (join_column_budget), and --max-join-pairs=N
// caps the join's work (max_join_pairs). A join truncated by that cap is not a proof, and more
// time does not lift it, so a run with it can end unproven. The forward pricer ignores all three.
//
// Pricing runs on a budget (vrp/pricing_budget.hpp): each solve starts with --pricing-budget
// seconds (default 0.05), and a solve that times out with no improving column is re-run on the
// same duals with ten times the budget, up to --pricing-timeout (default 120 s), so column
// generation still ends proven. --budget-reset starts each iteration from the base budget again.
// --pricing-budget=inf turns the budget off; --pricing-timeout is then a bare timeout, and every
// earlier measurement's command line runs as it did.
//
// --verify reruns column generation from scratch under the final grown table -- a model built
// with it, not one swapped into -- and checks it reaches the same bound: if removing columns had
// ever left an ng-infeasible column in the master, the dynamic bound would be lower.
//
// Needs Gurobi, and a build whose kRouteRelaxation is RouteRelaxation::NgPath.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "vrp/instance_reader.hpp"
#include "vrp/ng_growth.hpp"
#include "vrp/vrp.hpp"

static_assert(
    kRouteRelaxation == RouteRelaxation::NgPath,
    "ng_dynamic_cg grows ng neighbourhoods, so it needs a build whose kRouteRelaxation is "
    "RouteRelaxation::NgPath. Edit that constant in examples/cpp/vrp/vrp.hpp for the "
    "measuring build only, and rebuild.");

namespace {

using ListLC = LabelList<ResourceType>;
using Table = std::map<size_t, std::set<size_t>>;

/// @brief What the driver was asked to run.
struct Options {
        std::vector<size_t> fixed{0, 4, 8, 12};
        bool dynamic = true;
        size_t start = 0;
        bool reduce = false;
        bool bidirectional = false;
        double timeout_s = 120.0;  // as kPricingBudgetSeconds in ng_cg_comparison_main.cpp
        bool verify = false;
        double budget_s = 0.05;        ///< --pricing-budget=S; infinity for none
        bool budget_sticky = true;     ///< false with --budget-reset
        bool dynamic_h = false;        ///< --dynamic-h
        size_t join_budget = MAX_INT;  ///< --join-budget=K
        size_t max_pairs = MAX_INT;    ///< --max-join-pairs=N
};

/// @brief Records the seconds of every `solve()` of the algorithm it wraps, and how many timed
///        out or had their join truncated.
///
/// A decorator rather than a subclass: `create_algorithm` returns the algorithm behind its base
/// class (the bidirectional one is internal), and `VRP::solve` takes `Algorithm` pointers. The
/// pricing budget sets its timeout on the decorator (`set_timeout` is not virtual); the decorator
/// starts from the wrapped algorithm's own timeout and passes its current one on before each solve.
class Timed : public Algorithm<ResourceType, ListLC> {
    public:
        Timed(ResourceFactory<ResourceType>* resource_factory,
              std::unique_ptr<Algorithm<ResourceType, ListLC>> inner)
            : Algorithm<ResourceType, ListLC>(resource_factory, params_timed_as(*inner)),
              inner_(std::move(inner)) {}

        SolveResult solve(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            inner_->set_timeout(this->timeout());
            const auto started = std::chrono::steady_clock::now();
            SolveResult result = inner_->solve(graph, cost_upper_bound);
            seconds_.push_back(
                std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());
            timeouts_ += result.status == AlgorithmStatus::TIMEOUT ? 1 : 0;
            truncated_joins_ +=
                result.bidirectional.has_value() && result.bidirectional->join_truncated ? 1 : 0;
            return result;
        }

        [[nodiscard]] bool is_optimal() const override { return inner_->is_optimal(); }

        [[nodiscard]] const std::vector<double>& seconds() const { return seconds_; }
        [[nodiscard]] size_t timeouts() const { return timeouts_; }
        [[nodiscard]] size_t truncated_joins() const { return truncated_joins_; }
        [[nodiscard]] const Algorithm<ResourceType, ListLC>* inner() const { return inner_.get(); }

    protected:
        // Never called: solve() hands the search to the wrapped algorithm.
        void initialize_labels() override {}
        [[nodiscard]] size_t number_of_labels() const override { return 0; }
        void main_loop() override {}
        [[nodiscard]] std::list<Label<ResourceType>*> get_labels_at_sinks() const override {
            return {};
        }
        std::vector<size_t> get_path_arc_ids(const Label<ResourceType>& /*label*/) override {
            return {};
        }

    private:
        std::unique_ptr<Algorithm<ResourceType, ListLC>> inner_;
        std::vector<double> seconds_;
        size_t timeouts_ = 0;
        size_t truncated_joins_ = 0;
};

/// @brief One column generation's outcome.
struct CGRun {
        std::string label;
        double lp_cost = 0.0;
        int iterations = 0;
        double pricing_seconds = 0.0;
        double seconds = 0.0;
        bool proven_optimal = true;
        size_t escalations = 0;
        size_t timeouts = 0;
        size_t truncated_joins = 0;
        bool has_controller = false;
        size_t h_moves = 0;
        size_t h_observations = 0;
        size_t rounds = 1;
        double mean_ng = 0.0;
        size_t max_ng = 0;
        std::string stop;
        Table table;
};

/// @brief Mean and largest neighbourhood size over the nodes the table names.
std::pair<double, size_t> ng_sizes(const Table& table) {
    size_t total = 0;
    size_t largest = 0;
    for (const auto& [node_id, members] : table) {
        total += members.size();
        largest = std::max(largest, members.size());
    }
    return {table.empty() ? 0.0 : static_cast<double>(total) / static_cast<double>(table.size()),
            largest};
}

/// @brief The LP columns of @p solution, with their node sequences from @p master.
std::vector<LpColumn> lp_columns(const MasterProblem& master, const MPSolution& solution) {
    std::vector<LpColumn> columns;
    for (const auto& [path_id, path] : master.paths()) {
        const auto it = solution.value_by_var_id.find(path_id);
        columns.push_back({.value = it == solution.value_by_var_id.end() ? 0.0 : it->second,
                           .nodes = path.visited_nodes});
    }
    return columns;
}

/// @brief The between-convergences controller: records a round, applies the brakes, grows.
class Augmenter {
    public:
        Augmenter(VRP* vrp, const std::vector<double>* pricing_seconds, bool reduce)
            : vrp_(vrp), pricing_seconds_(pricing_seconds), reduce_(reduce) {}

        /// @brief The hook `VRP::solve` calls on every proven convergence.
        bool operator()(MasterProblem& master, const MPSolution& solution) {
            const double bound = solution.cost;
            const double seconds = seconds_since_last_round();
            const size_t iterations = pricing_seconds_->size() - solves_at_last_round_;
            solves_at_last_round_ = pricing_seconds_->size();
            // What the brakes compare: seconds per pricing solve, not per round.
            const double per_solve =
                iterations == 0 ? 0.0 : seconds / static_cast<double>(iterations);
            ++round_;
            const double delta = round_ == 1 ? 0.0 : bound - bounds_.back();
            bounds_.push_back(bound);

            std::string action;
            size_t removed = 0;
            size_t added = 0;
            const bool follows_reduce = baseline_pending_;
            const bool keep_going =
                decide(master, solution, bound, per_solve, delta, &action, &removed, &added);

            const auto [mean, largest] = ng_sizes(vrp_->ng_neighborhoods());
            std::cout << "      round " << std::setw(2) << round_ << "  bound=" << std::fixed
                      << std::setprecision(4) << bound << "  delta=" << std::showpos << delta
                      << std::noshowpos << (follows_reduce ? " (after reduce)" : "")
                      << "  solves=" << std::setw(3) << iterations
                      << "  pricing=" << std::setprecision(2) << seconds << " s"
                      << "  per-solve=" << std::setprecision(1) << 1000.0 * per_solve << " ms"
                      << "  removed=" << removed << "  added=" << added
                      << "  ng mean/max=" << std::setprecision(2) << mean << "/" << largest << "  "
                      << action << std::endl;
            if (!keep_going) {
                stop_ = action;
            }
            return keep_going;
        }

        [[nodiscard]] size_t rounds() const { return round_; }
        [[nodiscard]] const std::string& stop() const { return stop_; }

    private:
        /// @brief RouteOpt's order: hard brake, soft brake, tail-off, grow, round cap.
        bool decide(MasterProblem& master, const MPSolution& solution, double bound,
                    double per_solve, double delta, std::string* action, size_t* removed,
                    size_t* added) {
            if (rolled_back_) {
                *action = "stop: re-converged after rollback";
                return false;
            }
            if (round_ == 1) {
                reference_per_solve_ = per_solve;
            } else {
                if (per_solve > params_.hard_time_factor * reference_per_solve_) {
                    vrp_->set_ng_neighborhoods(previous_);
                    rolled_back_ = true;
                    *action = "rollback (hard brake)";
                    return true;
                }
                if (per_solve > params_.soft_time_factor * reference_per_solve_) {
                    *action = "stop: soft brake";
                    return false;
                }
                if (baseline_pending_) {
                    // A reduction lowers the bound by design; this round's bound is the new
                    // baseline, and later deltas are measured from it (bounds_.back()).
                    baseline_pending_ = false;
                } else {
                    if (delta <= 1e-9) {
                        *action = "stop: no improvement";
                        return false;
                    }
                    if (improving_rounds_ >= 2 && delta < params_.tail_off * best_delta_) {
                        *action = "stop: tail-off";
                        return false;
                    }
                    ++improving_rounds_;
                    best_delta_ = std::max(best_delta_, delta);
                }
            }
            if (round_ >= params_.max_rounds) {
                *action = "stop: rounds";
                return false;
            }

            const auto columns = lp_columns(master, solution);
            previous_ = vrp_->ng_neighborhoods();
            if (reduce_ && round_ == 1) {
                vrp_->set_ng_neighborhoods(reduce_to_support(previous_, columns));
                baseline_pending_ = true;
                *action = "reduce";
                return true;
            }
            const NgGrowth growth = grow_ng(previous_, columns, params_);
            if (growth.members_added == 0) {
                *action = "stop: nothing to add";
                return false;
            }
            vrp_->set_ng_neighborhoods(growth.neighborhoods);
            *removed = vrp_->remove_columns(&master, [&](const Path& path) {
                return !rcspp::is_ng_feasible(std::span<const size_t>(path.visited_nodes),
                                              growth.neighborhoods);
            });
            *added = growth.members_added;
            *action = "grow";
            (void)bound;
            return true;
        }

        double seconds_since_last_round() {
            double total = 0.0;
            for (size_t i = solves_at_last_seconds_; i < pricing_seconds_->size(); ++i) {
                total += (*pricing_seconds_)[i];
            }
            solves_at_last_seconds_ = pricing_seconds_->size();
            return total;
        }

        VRP* vrp_;
        const std::vector<double>* pricing_seconds_;
        bool reduce_;
        NgGrowthParams params_;
        size_t round_ = 0;
        size_t solves_at_last_round_ = 0;
        size_t solves_at_last_seconds_ = 0;
        std::vector<double> bounds_;
        double reference_per_solve_ = 0.0;  ///< round 1's seconds per pricing solve
        double best_delta_ = 0.0;
        size_t improving_rounds_ = 0;
        bool rolled_back_ = false;
        bool baseline_pending_ = false;  ///< the next round follows a reduce
        Table previous_;
        std::string stop_ = "converged";
};

/// @brief One column generation: fixed at the table @p vrp starts with, or growing it, priced by
///        the simple search, forward or (with `options.bidirectional`) bidirectional.
CGRun run(VRP& vrp, const Instance& instance, const Options& options, bool dynamic,
          std::string label) {
    AlgorithmParams<ListLC> params;
    params.timeout_s = options.timeout_s;
    if (std::isfinite(options.budget_s)) {
        // The budget times every solve, and the timeout becomes its cap.
        vrp.set_pricing_budget({.seconds = options.budget_s,
                                .max_seconds = options.timeout_s,
                                .sticky = options.budget_sticky});
        params.timeout_s = std::numeric_limits<double>::infinity();
    }
    if (options.bidirectional) {
        params.direction = SearchDirection::Bidirectional;
        params.critical_resource_index = 1;  // time
        params.half_way_point = static_cast<double>(instance.get_depot_customer().due_time) / 2.0;
        params.dynamic_half_way = options.dynamic_h;
        params.join_column_budget = options.join_budget;
        params.max_join_pairs = options.max_pairs;
    }
    Timed timed(&vrp.get_graph().get_resource_factory(),
                vrp.get_graph().create_algorithm<SimpleDominanceAlgorithm>(params));
    std::vector<Algorithm<ResourceType, ListLC>*> algorithms{&timed};

    Augmenter augmenter(&vrp, &timed.seconds(), options.reduce);
    ConvergedHook hook;
    if (dynamic) {
        std::cout << "    " << label << std::endl;
        hook = [&augmenter](MasterProblem& master, const MPSolution& solution) {
            return augmenter(master, solution);
        };
    }

    const auto started = std::chrono::steady_clock::now();
    const auto result = vrp.solve<>(params, std::nullopt, algorithms, false, {}, hook);
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

    CGRun out{.label = std::move(label),
              .lp_cost = result.lp_cost,
              .iterations = result.iterations,
              .pricing_seconds = 0.0,
              .seconds = seconds,
              .proven_optimal = result.proven_optimal,
              .escalations = result.escalations,
              .rounds = dynamic ? augmenter.rounds() : 1,
              .table = vrp.ng_neighborhoods()};
    for (const double s : timed.seconds()) {
        out.pricing_seconds += s;
    }
    out.timeouts = timed.timeouts();
    out.truncated_joins = timed.truncated_joins();
    // Only the bidirectional pricer has a controller.
    if (const auto* bidirectional = as_bidirectional(timed.inner())) {
        const auto& controller = bidirectional->half_way_controller();
        out.has_controller = controller.has_initial_h();
        out.h_moves = controller.moves();
        out.h_observations = controller.observations();
    }
    std::tie(out.mean_ng, out.max_ng) = ng_sizes(out.table);
    out.stop = dynamic ? augmenter.stop() : "fixed";
    return out;
}

CGRun run_with_pricer(VRP& vrp, const Instance& instance, const Options& options, bool dynamic,
                      std::string label) {
    return run(vrp, instance, options, dynamic, std::move(label));
}

void print(const CGRun& run) {
    std::cout << "    " << std::left << std::setw(12) << run.label << std::right
              << " lp_cost=" << std::fixed << std::setprecision(4) << run.lp_cost
              << "  iterations=" << run.iterations << "  rounds=" << run.rounds
              << "  pricing=" << std::setprecision(2) << run.pricing_seconds << " s"
              << "  total=" << run.seconds << " s  escalations=" << run.escalations
              << "  timeouts=" << run.timeouts << "  jtrunc=" << run.truncated_joins
              << (run.has_controller ? "  H moves/obs=" + std::to_string(run.h_moves) + "/" +
                                           std::to_string(run.h_observations)
                                     : std::string())
              << "  ng mean/max=" << run.mean_ng << "/" << run.max_ng << "  " << run.stop
              << (run.proven_optimal ? "" : "  (NOT proven optimal)") << std::endl;
}

std::vector<size_t> parse_sizes(const std::string& csv) {
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

}  // namespace

int main(int argc, char** argv) {
    Options options;
    std::vector<std::string> names;
    const auto value_of = [](const std::string& arg) { return arg.substr(arg.find('=') + 1); };
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.starts_with("--fixed=")) {
            options.fixed = parse_sizes(value_of(arg));
        } else if (arg == "--no-dynamic") {
            options.dynamic = false;
        } else if (arg.starts_with("--start=")) {
            options.start = std::stoul(value_of(arg));
        } else if (arg == "--reduce") {
            options.reduce = true;
        } else if (arg.starts_with("--pricer=")) {
            options.bidirectional = value_of(arg) == "bidirectional";
        } else if (arg.starts_with("--pricing-timeout=")) {
            options.timeout_s = std::stod(value_of(arg));
        } else if (arg.starts_with("--pricing-budget=")) {
            options.budget_s = std::stod(value_of(arg));
        } else if (arg == "--budget-reset") {
            options.budget_sticky = false;
        } else if (arg == "--dynamic-h") {
            options.dynamic_h = true;
        } else if (arg.starts_with("--join-budget=")) {
            options.join_budget = std::stoull(value_of(arg));
        } else if (arg.starts_with("--max-join-pairs=")) {
            options.max_pairs = std::stoull(value_of(arg));
        } else if (arg == "--verify") {
            options.verify = true;
        } else {
            names.push_back(arg);
        }
    }
    if (names.empty()) {
        names = {"R201_25", "RC201_50", "C201_50"};
    }

    std::cout << "[ CG ] dynamic ng augmentation against fixed ng sizes, full column generation\n"
              << "       pricer=" << (options.bidirectional ? "bidirectional" : "forward")
              << "  pricing_timeout=" << options.timeout_s << " s  start=ng(" << options.start
              << ")" << (options.reduce ? " + reduce" : "") << "\n"
              << "       pricing_budget="
              << (std::isfinite(options.budget_s)
                      ? std::to_string(options.budget_s) + " s, x10 on an empty timeout up to " +
                            "the timeout" +
                            (options.budget_sticky ? ", sticky" : ", reset each iteration")
                      : std::string("none (the timeout is bare)"))
              << "\n";
    const auto limit = [](size_t value) {
        return value >= MAX_INT ? std::string("none") : std::to_string(value);
    };
    if (options.bidirectional) {
        std::cout << "       dynamic_h=" << (options.dynamic_h ? "yes" : "no")
                  << "  join_budget=" << limit(options.join_budget)
                  << "  max_join_pairs=" << limit(options.max_pairs) << "\n";
    } else if (options.dynamic_h || options.join_budget < MAX_INT || options.max_pairs < MAX_INT) {
        std::cout << "       (--dynamic-h, --join-budget and --max-join-pairs are ignored by the "
                     "forward pricer)\n";
    }
    std::cout << std::endl;

    for (const auto& name : names) {
        const std::string root_dir = file_parent_dir(__FILE__, 3);
        InstanceReader reader(root_dir + "/instances/" + name + ".txt");
        const auto instance = reader.read();
        std::cout << "  " << name << std::endl;

        std::vector<CGRun> fixed;
        for (const size_t size : options.fixed) {
            VRP vrp(instance, size);
            fixed.push_back(
                run_with_pricer(vrp, instance, options, false, "ng(" + std::to_string(size) + ")"));
            print(fixed.back());
        }

        if (!options.dynamic) {
            std::cout << std::endl;
            continue;
        }
        VRP vrp(instance, options.start);
        const CGRun dynamic = run_with_pricer(vrp,
                                              instance,
                                              options,
                                              true,
                                              "dynamic(" + std::to_string(options.start) + ")");
        print(dynamic);

        if (options.verify) {
            VRP fresh(instance, dynamic.table);
            const CGRun check = run_with_pricer(fresh, instance, options, false, "verify");
            print(check);
            const double gap = std::abs(check.lp_cost - dynamic.lp_cost);
            std::cout << "    verify: " << (gap <= 1e-6 ? "same bound" : "*** DIFFERENT BOUND")
                      << " (gap " << std::scientific << std::setprecision(2) << gap << ")"
                      << std::fixed << std::endl;
        }

        // Against the fixed sizes: the best bound, and the cheapest size that reaches ours. Only
        // proven runs count: an unproven LP value sits above the optimum, so it would pass for a
        // better bound than it is.
        std::vector<const CGRun*> proven;
        for (const auto& run : fixed) {
            if (run.proven_optimal) {
                proven.push_back(&run);
            }
        }
        const size_t unproven = fixed.size() - proven.size();
        if (!fixed.empty() && !dynamic.proven_optimal) {
            std::cout << "    compare: skipped, the dynamic run is not proven" << std::endl;
        } else if (!fixed.empty() && proven.empty()) {
            std::cout << "    compare: no fixed run was proven (" << unproven << " unproven)"
                      << std::endl;
        } else if (!fixed.empty()) {
            const CGRun* best = *std::ranges::max_element(
                proven,
                [](const CGRun* a, const CGRun* b) { return a->lp_cost < b->lp_cost; });
            const CGRun* cheapest = nullptr;
            for (const CGRun* run : proven) {
                if (run->lp_cost >= dynamic.lp_cost - 1e-4 &&
                    (cheapest == nullptr || run->seconds < cheapest->seconds)) {
                    cheapest = run;
                }
            }
            std::cout << "    compare: dynamic " << std::setprecision(4) << dynamic.lp_cost
                      << " vs best fixed " << best->lp_cost << " (" << best->label << ", gap "
                      << std::showpos << dynamic.lp_cost - best->lp_cost << std::noshowpos << ")";
            if (cheapest != nullptr) {
                std::cout << "; time " << std::setprecision(2) << dynamic.seconds << " s vs "
                          << cheapest->seconds << " s for " << cheapest->label
                          << ", the cheapest fixed size reaching it ("
                          << dynamic.seconds / std::max(cheapest->seconds, 1e-9) << "x)";
            } else {
                std::cout << "; no fixed size reaches the dynamic bound";
            }
            if (unproven > 0) {
                std::cout << " (" << unproven << " unproven fixed run(s) ignored)";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
    return 0;
}
