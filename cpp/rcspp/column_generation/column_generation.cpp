// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/column_generation/column_generation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace rcspp::column_generation {

namespace {

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
}

ColumnGenerationResult ColumnGeneration::run() {
    ColumnGenerationResult result;
    Timer total_timer(true);
    Timer relaxation_timer;
    Timer pricing_timer;
    const bool timers = params_.statistics.timers;

    // The master is initialized once, by the first run.
    if (!initialized_) {
        context_ = compute_tolerance_context();
        params_.initialization->initialize(master_, context_);
        initialized_ = true;
    }

    const double reduced_cost_tolerance = params_.tolerances->reduced_cost(context_);
    SolverBackend& backend = master_.backend();
    std::vector<double> duals;

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
            timed(timers, relaxation_timer, [&] { return backend.solve_relaxation(relaxation); });
        if (solver_status != SolverStatus::Optimal) {
            result.status = status_after_failed_solve(solver_status);
            break;
        }

        // Read its value and duals.
        ++result.iterations;
        result.objective = backend.objective();
        duals.resize(master_.num_rows());
        backend.duals(duals);

        // Price the duals and add the columns found.
        const auto [new_columns, proven] = timed(timers, pricing_timer, [&] {
            return price(duals, total_timer, reduced_cost_tolerance);
        });
        result.statistics.generated_columns += new_columns;

        if (params_.on_iteration) {
            params_.on_iteration(IterationInfo{.iteration = result.iterations,
                                               .objective = result.objective,
                                               .new_columns = new_columns,
                                               .total_columns = master_.num_columns(),
                                               .elapsed_seconds = total_timer.elapsed_seconds()});
        }

        // New columns: solve again. None: converged, proven or not.
        if (new_columns > 0) {
            continue;
        }

        if (!proven) {
            result.status = ColumnGenerationStatus::NotProven;
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
            if (params_.on_converged(context) == ConvergenceAction::Continue) {
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
    result.statistics.total_seconds = total_timer.elapsed_seconds();
    result.statistics.relaxation_seconds = relaxation_timer.elapsed_seconds();
    result.statistics.pricing_seconds = pricing_timer.elapsed_seconds();
    return result;
}

ColumnGeneration::PricingRound ColumnGeneration::price(std::span<const double> duals,
                                                       const Timer& total_timer,
                                                       double reduced_cost_tolerance) {
    PricingRound round{.new_columns = 0, .proven = true};

    for (size_t index = 0; index < master_.num_pricers(); ++index) {
        // A column improves when its reduced cost, convexity dual included, is negative.
        const std::optional<size_t> convexity = master_.convexity_row(index);
        const double convexity_dual = convexity.has_value() ? duals[*convexity] : 0.0;
        const PricingRequest request{
            .reduced_cost_threshold = convexity_dual - reduced_cost_tolerance,
            .timeout_s = std::max(0.0, params_.timeout_s - total_timer.elapsed_seconds())};

        PricingResult priced = master_.pricer(index).price(duals, request);

        round.proven =
            round.proven && priced.status == PricingStatus::Optimal && priced.exact_stage;
        round.new_columns += priced.columns.size();
        master_.add_columns(index, std::move(priced.columns));
    }

    return round;
}

ToleranceContext ColumnGeneration::compute_tolerance_context() const {
    double scale = 1.0;

    for (size_t index = 0; index < master_.num_pricers(); ++index) {
        scale = std::max(scale, master_.pricer(index).cost_scale());
    }

    for (const MasterColumn& column : master_.columns()) {
        scale = std::max(scale, std::abs(column.solution.column.cost));
    }

    return ToleranceContext{.cost_scale = scale};
}

std::vector<double> ColumnGeneration::column_values() const {
    std::vector<ColumnId> ids(master_.num_columns());
    for (size_t index = 0; index < ids.size(); ++index) {
        ids[index] = master_.columns()[index].id;
    }

    std::vector<double> values(ids.size());
    master_.backend().primal(ids, values);
    return values;
}

bool ColumnGeneration::uses_artificial_columns() const {
    const std::span<const ColumnId> ids = master_.artificial_column_ids();
    std::vector<double> values(ids.size());
    master_.backend().primal(ids, values);

    const double tolerance = params_.tolerances->feasibility(context_);
    return std::ranges::any_of(values, [tolerance](double value) { return value > tolerance; });
}

}  // namespace rcspp::column_generation
