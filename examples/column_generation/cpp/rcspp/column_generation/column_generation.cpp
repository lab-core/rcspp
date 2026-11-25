// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/column_generation/column_generation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "rcspp/column_generation/pricer.hpp"

namespace rcspp::column_generation {

namespace {

constexpr double infinity = std::numeric_limits<double>::infinity();

// How a run ends when the master LP was not solved to optimality. Every status is listed, without
// a default, so that a new SolverStatus value makes the compiler warn here.
ColumnGenerationStatus status_after_failed_solve(SolverStatus status) {
    switch (status) {
        case SolverStatus::Infeasible:
            return ColumnGenerationStatus::RestrictedMasterInfeasible;
        case SolverStatus::Unbounded:
            return ColumnGenerationStatus::Unbounded;
        case SolverStatus::TimeLimit:
            return ColumnGenerationStatus::TimeLimit;
        case SolverStatus::Optimal:  // not a failure; never passed here
        case SolverStatus::Other:
            return ColumnGenerationStatus::SolverError;
    }
    return ColumnGenerationStatus::SolverError;  // an out-of-range value
}

// Calls `function`, adding its duration to `timer` when timing is on.
template <typename Function>
auto timed(bool enabled, Timer& timer, Function&& function) {
    if (enabled) {
        timer.start();
    }
    auto value = std::forward<Function>(function)();
    if (enabled) {
        timer.stop();
    }
    return value;
}

}  // namespace

ColumnGeneration::ColumnGeneration(MasterProblem& master, ColumnGenerationParams params)
    : master_(master), params_(std::move(params)) {
    if (params_.tolerances == nullptr) {
        params_.tolerances = std::make_shared<AbsoluteTolerances>();
    }

    if (params_.initialization == nullptr) {
        params_.initialization = std::make_shared<ArtificialColumns>();
    }

    // A negative gap would silently never stop the run.
    if (!(params_.absolute_gap >= 0.0)) {
        throw std::invalid_argument("ColumnGeneration: absolute_gap must be at least 0");
    }

    if (!(params_.relative_gap >= 0.0)) {
        throw std::invalid_argument("ColumnGeneration: relative_gap must be at least 0");
    }
}

ColumnGenerationResult ColumnGeneration::run() {
    ColumnGenerationResult result;
    try {
        result = iterate();
    } catch (...) {
        master_.solver().restore_user_model();
        throw;
    }
    master_.solver().restore_user_model();
    return result;
}

ColumnGenerationResult ColumnGeneration::iterate() {
    ColumnGenerationResult result;
    Timer total_timer(true);
    Timer relaxation_timer;
    Timer pricing_timer;
    const bool timers = params_.statistics.timers;

    initialize();

    // Never below the LP solver's own tolerance, or a column it leaves out would come back forever.
    MasterSolver& solver = master_.solver();
    const double reduced_cost_tolerance =
        std::max(params_.tolerances->reduced_cost(context_), solver.dual_tolerance());
    std::vector<double> duals;
    std::vector<double> paths = paths_allowed();

    while (true) {
        // Limits.
        if (result.iterations >= params_.max_iterations) {
            result.status = ColumnGenerationStatus::IterationLimit;
            break;
        }

        const double remaining = params_.timeout_s - total_timer.elapsed_seconds();
        if (remaining <= 0.0) {
            result.status = ColumnGenerationStatus::TimeLimit;
            break;
        }

        // Solve the master LP, within the time left.
        RelaxationParams relaxation = params_.relaxation;
        relaxation.timeout_s = std::min(relaxation.timeout_s, remaining);

        const SolverStatus solver_status =
            timed(timers, relaxation_timer, [&] { return solver.solve_relaxation(relaxation); });
        if (solver_status != SolverStatus::Optimal) {
            result.status = status_after_failed_solve(solver_status);
            break;
        }

        // Read its value and duals.
        ++result.iterations;
        result.objective = solver.objective();
        duals.resize(master_.number_of_rows());
        solver.duals(duals);

        // Price the duals and add the columns found.
        const auto [new_columns, proven, bound_terms] = timed(timers, pricing_timer, [&] {
            return price(duals, total_timer, reduced_cost_tolerance, paths);
        });
        result.statistics.generated_columns += new_columns;
        const double iteration_bound = result.objective + bound_terms;
        result.lower_bound = std::max(result.lower_bound, iteration_bound);

        if (params_.on_iteration) {
            params_.on_iteration(IterationInfo{.iteration = result.iterations,
                                               .objective = result.objective,
                                               .lower_bound = iteration_bound,
                                               .new_columns = new_columns,
                                               .total_columns = master_.number_of_columns(),
                                               .elapsed_seconds = total_timer.elapsed_seconds()});
        }

        // New columns: solve again, unless the bound is already close enough.
        if (new_columns > 0) {
            const std::optional<ColumnGenerationStatus> stop =
                early_stop(result.objective, iteration_bound, reduced_cost_tolerance);
            if (stop.has_value()) {
                result.status = *stop;
                break;
            }
            continue;
        }

        // Unproven: the run's time limit may have cut pricing short.
        if (!proven) {
            result.status = total_timer.elapsed_seconds() >= params_.timeout_s
                                ? ColumnGenerationStatus::TimeLimit
                                : ColumnGenerationStatus::NotProven;
            break;
        }

        // Proven convergence: the hook may change a pricer and ask for more.
        if (params_.on_converged) {
            const std::vector<double> values = column_values();
            ConvergenceContext context{.master = master_,
                                       .iteration = result.iterations,
                                       .objective = result.objective,
                                       .duals = duals,
                                       .values = values};
            // The hook may have changed the problem: the bounds found so far may not hold.
            if (params_.on_converged(context) == ConvergenceAction::Continue) {
                result.lower_bound = -infinity;
                paths = paths_allowed();
                continue;
            }
        }

        // An artificial column still in use means no real solution exists.
        if (uses_artificial_columns()) {
            result.status = ColumnGenerationStatus::Infeasible;
        } else {
            result.status = ColumnGenerationStatus::Optimal;
            result.lower_bound = result.objective;
        }
        break;
    }

    result.statistics.artificial_columns = master_.artificial_column_ids().size();
    result.statistics.artificial_cost = master_.artificial_cost();
    result.statistics.total_seconds = total_timer.elapsed_seconds();
    result.statistics.relaxation_seconds = relaxation_timer.elapsed_seconds();
    result.statistics.pricing_seconds = pricing_timer.elapsed_seconds();
    return result;
}

void ColumnGeneration::initialize() {
    if (initialized_) {
        return;
    }
    context_ = compute_tolerance_context();
    if (!master_.initialized()) {
        params_.initialization->initialize(master_, context_);
        master_.mark_initialized();
    }
    initialized_ = true;
}

// The bound: for any solution of the full master, cost = LP value + the sum over the columns of
// reduced cost * value. A pricer's columns cost at least its best reduced cost each, and its
// convexity row limits how many a solution uses (paths_allowed), so each pricer adds (paths
// allowed) * (its best reduced cost) when that is negative.
ColumnGeneration::PricingRound ColumnGeneration::price(std::span<const double> duals,
                                                       const Timer& total_timer,
                                                       double reduced_cost_tolerance,
                                                       std::span<const double> paths_allowed) {
    PricingRound round{.new_columns = 0, .proven = true, .bound_terms = 0.0};

    for (size_t index = 0; index < master_.number_of_pricers(); ++index) {
        // A column improves when its reduced cost, convexity dual included, is negative.
        const std::optional<size_t> convexity = master_.convexity_row(index);
        const double convexity_dual = convexity.has_value() ? duals[*convexity] : 0.0;
        const PricingRequest request{
            .reduced_cost_threshold = convexity_dual - reduced_cost_tolerance,
            .timeout_s = std::max(0.0, params_.timeout_s - total_timer.elapsed_seconds())};

        PricingResult priced = master_.pricer(index).price(duals, request);

        // Only an exact stage that finished proves there is no improving column. No path at all
        // (Infeasible) proves it too: no column of any cost exists. A pricer with nothing to
        // price holds no improving column.
        const bool finished =
            priced.status == PricingStatus::Optimal || priced.status == PricingStatus::Infeasible;
        const bool nothing_to_price = priced.status == PricingStatus::NotApplicable;
        const bool proved = nothing_to_price || (priced.exact_stage && finished);
        round.proven = round.proven && proved;

        // Nothing to add to the bound when the pricer proved it has no improving column (its
        // bound may be the threshold, a tolerance below zero), or when it is already unknown.
        if (!(proved && priced.columns.empty()) && round.bound_terms > -infinity) {
            // Its best reduced cost, convexity dual included; never above a column it returned.
            double best = priced.min_reduced_cost_bound;
            if (best > -infinity) {
                for (const Solution& column : priced.columns) {
                    best = std::min(best, column.cost);
                }
            }
            best -= convexity_dual;

            // An unknown best (-infinity) or no limit on its paths (infinity) makes the bound
            // unknown; a row that allows no path adds nothing.
            if (best < 0.0 && paths_allowed[index] > 0.0) {
                round.bound_terms += paths_allowed[index] * best;
            }
        }

        round.new_columns += priced.columns.size();
        master_.add_columns(index, std::move(priced.columns));
    }

    return round;
}

std::vector<double> ColumnGeneration::paths_allowed() const {
    const size_t pricers = master_.number_of_pricers();
    std::vector<double> paths(pricers, infinity);

    bool any_row = false;
    for (size_t index = 0; index < pricers; ++index) {
        any_row = any_row || master_.convexity_row(index).has_value();
    }
    if (!any_row) {
        return paths;
    }

    const size_t rows = master_.number_of_rows();
    std::vector<double> lower(rows);
    std::vector<double> upper(rows);
    master_.solver().row_bounds(lower, upper);

    for (size_t index = 0; index < pricers; ++index) {
        const std::optional<size_t> row = master_.convexity_row(index);
        if (row.has_value()) {
            paths[index] = upper[*row];
        }
    }
    return paths;
}

std::optional<ColumnGenerationStatus> ColumnGeneration::early_stop(double objective, double bound,
                                                                   double tolerance) const {
    if (bound == -infinity) {
        return std::nullopt;
    }

    // Nothing here costs less than the bound; with integer costs, than the bound rounded up.
    const double lowest_cost =
        params_.integer_objective ? std::ceil(bound - tolerance) : bound + tolerance;

    if (lowest_cost >= params_.cutoff) {
        return ColumnGenerationStatus::Cutoff;
    }

    const double gap = objective - bound;

    if (params_.absolute_gap > 0.0 && gap <= params_.absolute_gap) {
        return ColumnGenerationStatus::AbsoluteGapLimit;
    }

    if (params_.relative_gap > 0.0 && gap <= params_.relative_gap * std::abs(objective)) {
        return ColumnGenerationStatus::RelativeGapLimit;
    }

    // More iterations cannot raise the bound above the LP value rounded up.
    if (params_.integer_objective && lowest_cost >= std::ceil(objective - tolerance)) {
        return ColumnGenerationStatus::IntegerObjectiveGap;
    }

    return std::nullopt;
}

ToleranceContext ColumnGeneration::compute_tolerance_context() const {
    double scale = 1.0;

    for (size_t index = 0; index < master_.number_of_pricers(); ++index) {
        scale = std::max(scale, master_.pricer(index).cost_scale());
    }

    for (const MasterColumn& column : master_.columns()) {
        scale = std::max(scale, std::abs(column.solution.column.cost));
    }

    // Starting columns the user built in their own model.
    scale = std::max(scale, master_.solver().largest_user_cost());

    return ToleranceContext{.cost_scale = scale};
}

std::vector<double> ColumnGeneration::column_values() const {
    std::vector<ColumnId> ids(master_.number_of_columns());
    for (size_t index = 0; index < ids.size(); ++index) {
        ids[index] = master_.columns()[index].id;
    }

    std::vector<double> values(ids.size());
    master_.solver().primal(ids, values);
    return values;
}

bool ColumnGeneration::uses_artificial_columns() const {
    const std::span<const ColumnId> ids = master_.artificial_column_ids();
    std::vector<double> values(ids.size());
    master_.solver().primal(ids, values);

    const double tolerance = params_.tolerances->feasibility(context_);
    return std::ranges::any_of(values, [tolerance](double value) { return value > tolerance; });
}

}  // namespace rcspp::column_generation
