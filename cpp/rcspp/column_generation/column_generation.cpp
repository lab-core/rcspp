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
    MasterSolver& solver = master_.solver();
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

        if (timers) {
            relaxation_timer.start();
        }
        const SolverStatus solver_status = solver.solve_relaxation(relaxation);
        if (timers) {
            relaxation_timer.stop();
        }

        if (solver_status != SolverStatus::Optimal) {
            result.status = solver_status == SolverStatus::TimeLimit
                                ? ColumnGenerationStatus::TimeLimit
                                : ColumnGenerationStatus::SolverError;
            break;
        }

        // Read its value and duals.
        ++result.iterations;
        result.objective = solver.objective();
        duals.resize(master_.number_of_rows());
        solver.duals(duals);

        // Price the duals and add the columns found.
        if (timers) {
            pricing_timer.start();
        }
        const auto [new_columns, proven] = price(duals, total_timer, reduced_cost_tolerance);
        if (timers) {
            pricing_timer.stop();
        }
        result.statistics.generated_columns += new_columns;

        if (params_.on_iteration) {
            params_.on_iteration(IterationInfo{.iteration = result.iterations,
                                               .objective = result.objective,
                                               .new_columns = new_columns,
                                               .total_columns = master_.number_of_columns(),
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

    for (size_t index = 0; index < master_.number_of_pricers(); ++index) {
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

    for (size_t index = 0; index < master_.number_of_pricers(); ++index) {
        scale = std::max(scale, master_.pricer(index).cost_scale());
    }

    for (const MasterColumn& column : master_.columns()) {
        scale = std::max(scale, std::abs(column.solution.column.cost));
    }

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
