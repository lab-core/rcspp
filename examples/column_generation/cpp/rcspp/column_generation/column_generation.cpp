// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/column_generation/column_generation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "rcspp/column_generation/pricer.hpp"
#include "rcspp/utils/logger.hpp"

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

// The name of a status, as the Python layers spell it. Every status is listed, without a default,
// so that a new value makes the compiler warn here.
std::string_view status_name(ColumnGenerationStatus status) {
    switch (status) {
        case ColumnGenerationStatus::Optimal:
            return "Optimal";
        case ColumnGenerationStatus::NotProven:
            return "NotProven";
        case ColumnGenerationStatus::Infeasible:
            return "Infeasible";
        case ColumnGenerationStatus::RestrictedMasterInfeasible:
            return "RestrictedMasterInfeasible";
        case ColumnGenerationStatus::Unbounded:
            return "Unbounded";
        case ColumnGenerationStatus::TimeLimit:
            return "TimeLimit";
        case ColumnGenerationStatus::IterationLimit:
            return "IterationLimit";
        case ColumnGenerationStatus::Cutoff:
            return "Cutoff";
        case ColumnGenerationStatus::AbsoluteGapLimit:
            return "AbsoluteGapLimit";
        case ColumnGenerationStatus::RelativeGapLimit:
            return "RelativeGapLimit";
        case ColumnGenerationStatus::IntegerObjectiveGap:
            return "IntegerObjectiveGap";
        case ColumnGenerationStatus::SolverError:
            return "SolverError";
    }
    return "Unknown";  // an out-of-range value
}

// A number of the log, or "-" when unknown.
std::string logged(double value, bool known) {
    return known ? std::format("{:.6g}", value) : std::string("-");
}

// A time of the log, or "-" when not measured.
std::string logged_seconds(const std::optional<double>& seconds) {
    return seconds.has_value() ? std::format("{:.3f} s", *seconds) : std::string("-");
}

// One line of the progress log.
void log_iteration(const IterationInfo& info) {
    const bool known = info.lower_bound > -infinity;
    const double gap = info.objective - info.lower_bound;
    const std::string relative =
        info.objective != 0.0 ? std::format(" ({:.2f}%)", 100.0 * gap / std::abs(info.objective))
                              : std::string();
    LOG_INFO(std::format("iteration {}  {:.2f} s  LP {}  pricing {}  columns +{} / {}  LP value "
                         "{:.6g}  bound {}  gap {}",
                         info.iteration,
                         info.elapsed_seconds,
                         logged_seconds(info.relaxation_seconds),
                         logged_seconds(info.pricing_seconds),
                         info.new_columns,
                         info.total_columns,
                         info.objective,
                         logged(info.lower_bound, known),
                         known ? logged(gap, true) + relative : std::string("-")),
             "\n");
}

// The last line of the progress log: how the run ended.
void log_end(const ColumnGenerationResult& result) {
    const Statistics& statistics = result.statistics;
    LOG_INFO(std::format("{} after {} iterations, {:.2f} s (LP {:.2f} s, pricing {:.2f} s), {} "
                         "columns generated, LP value {}, bound {}",
                         status_name(result.status),
                         result.iterations,
                         statistics.total_seconds,
                         statistics.relaxation_seconds,
                         statistics.pricing_seconds,
                         statistics.generated_columns,
                         logged(result.objective, result.iterations > 0),
                         logged(result.lower_bound, result.lower_bound > -infinity)),
             "\n");
}

// What a pricer did, from its answer; the engine's counts dropped unless collected.
PricerIterationInfo pricer_info(PricingResult& priced, double convexity_dual, double seconds,
                                const StatisticsParams& statistics) {
    PricerIterationInfo info{.status = priced.status,
                             .new_columns = priced.columns.size(),
                             .min_reduced_cost = std::nullopt,
                             .seconds = seconds,
                             .search_details = std::move(priced.search_details)};
    if (!priced.columns.empty()) {
        double best = infinity;
        for (const Solution& column : priced.columns) {
            best = std::min(best, column.cost);
        }
        info.min_reduced_cost = best - convexity_dual;
    }
    if (!statistics.search) {
        for (PricingStageInfo& stage : info.search_details) {
            stage.extended_labels.reset();
            stage.bidirectional.reset();
        }
    }
    return info;
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

    // The counts sit in the pricers' details: alone, they would silently give nothing.
    if (params_.statistics.search && !params_.statistics.pricers) {
        throw std::invalid_argument("ColumnGeneration: the search statistics need the pricers'");
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
    const StatisticsParams& statistics = params_.statistics;
    const bool timers = statistics.timers;
    const bool wants_info = params_.on_iteration || params_.log_frequency > 0;
    std::optional<IterationInfo> last_info;
    const DebugChecks& checks = params_.debug_checks;
    const bool checks_solution =
        checks.dual_signs || checks.master_reduced_costs || checks.objective;

    initialize();
    row_lower_.clear();
    row_upper_.clear();
    user_cost_.reset();

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

        const double relaxation_before = relaxation_timer.elapsed_seconds();
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

        if (checks_solution) {
            check_solution(result.iterations, result.objective, duals, reduced_cost_tolerance);
        }

        // The LP solution, read before the new columns change the model.
        std::optional<std::pair<std::vector<ColumnId>, std::vector<double>>> master_values;
        if (wants_info && statistics.master_values) {
            master_values = nonzero_values();
        }

        // Price the duals and add the columns found.
        const size_t columns_before = master_.number_of_columns();
        const double relaxation_seconds = relaxation_timer.elapsed_seconds() - relaxation_before;
        const double pricing_before = pricing_timer.elapsed_seconds();
        PricingRound round = timed(timers, pricing_timer, [&] {
            return price(duals,
                         total_timer,
                         reduced_cost_tolerance,
                         paths,
                         wants_info,
                         result.iterations);
        });
        const size_t new_columns = round.new_columns;
        const bool proven = round.proven;
        result.statistics.generated_columns += new_columns;
        const double iteration_bound = result.objective + round.bound_terms;
        result.lower_bound = std::max(result.lower_bound, iteration_bound);

        if (wants_info) {
            IterationInfo info{.iteration = result.iterations,
                               .objective = result.objective,
                               .lower_bound = iteration_bound,
                               .new_columns = new_columns,
                               .total_columns = master_.number_of_columns(),
                               .elapsed_seconds = total_timer.elapsed_seconds()};
            if (timers) {
                info.relaxation_seconds = relaxation_seconds;
                info.pricing_seconds = pricing_timer.elapsed_seconds() - pricing_before;
            }
            info.pricers = std::move(round.pricers);
            if (statistics.duals) {
                info.duals = duals;
            }
            info.master_values = std::move(master_values);
            if (statistics.columns) {
                const std::span<const MasterColumn> added =
                    master_.columns().subspan(columns_before);
                info.added_columns.emplace(added.begin(), added.end());
            }

            if (params_.log_frequency > 0 && result.iterations % params_.log_frequency == 0) {
                log_iteration(info);
            }
            if (params_.on_iteration) {
                params_.on_iteration(info);
            }
            last_info = std::move(info);
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
                row_lower_.clear();
                row_upper_.clear();
                user_cost_.reset();
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

    if (params_.log_frequency > 0) {
        // The last iteration, unless its line is out already, then how the run ended.
        if (last_info.has_value() && last_info->iteration % params_.log_frequency != 0) {
            log_iteration(*last_info);
        }
        log_end(result);
    }
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
                                                       std::span<const double> paths_allowed,
                                                       bool wants_info, size_t iteration) {
    const StatisticsParams& statistics = params_.statistics;
    const bool check_columns = params_.debug_checks.pricer_reduced_costs;
    const bool collect = wants_info && statistics.pricers;
    PricingRound round{.new_columns = 0, .proven = true, .bound_terms = 0.0, .pricers = {}};
    if (collect) {
        round.pricers.emplace();
    }

    for (size_t index = 0; index < master_.number_of_pricers(); ++index) {
        // A column improves when its reduced cost, convexity dual included, is negative.
        const std::optional<size_t> convexity = master_.convexity_row(index);
        const double convexity_dual = convexity.has_value() ? duals[*convexity] : 0.0;
        const double pricing_start = total_timer.elapsed_seconds();
        const PricingRequest request{
            .reduced_cost_threshold = convexity_dual - reduced_cost_tolerance,
            .timeout_s = std::max(0.0, params_.timeout_s - pricing_start),
            .search_details = collect};

        PricingResult priced = master_.pricer(index).price(duals, request);

        if (collect) {
            round.pricers->push_back(pricer_info(priced,
                                                 convexity_dual,
                                                 total_timer.elapsed_seconds() - pricing_start,
                                                 statistics));
        }

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

        if (check_columns) {
            check_pricer_reduced_costs(priced.columns,
                                       duals,
                                       reduced_cost_tolerance,
                                       iteration,
                                       index);
        }

        round.new_columns += priced.columns.size();
        master_.add_columns(index, std::move(priced.columns));
    }

    return round;
}

void ColumnGeneration::check_solution(size_t iteration, double objective,
                                      std::span<const double> duals, double tolerance) {
    const DebugChecks& checks = params_.debug_checks;
    MasterSolver& solver = master_.solver();

    // The rows' bounds, read again when rows were added.
    if (checks.dual_signs) {
        if (row_lower_.size() != duals.size()) {
            row_lower_.resize(duals.size());
            row_upper_.resize(duals.size());
            solver.row_bounds(row_lower_, row_upper_);
        }

        check_dual_signs(duals, row_lower_, row_upper_, tolerance, iteration);
    }

    if (!checks.master_reduced_costs && !checks.objective) {
        return;
    }

    keep_new_columns(kept_columns_, master_);
    const std::vector<double> values = column_values();

    if (checks.master_reduced_costs) {
        const double feasibility = params_.tolerances->feasibility(context_);
        check_master_reduced_costs(kept_columns_, duals, values, tolerance, feasibility, iteration);
    }

    // The LP value is its columns' cost only when the user's own variables cost nothing.
    if (checks.objective) {
        if (!user_cost_.has_value()) {
            user_cost_ = solver.largest_user_cost();
        }

        if (*user_cost_ == 0.0) {
            check_objective(kept_columns_, values, objective, tolerance, iteration);
        }
    }
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
        if (column.tag == ColumnTag::Real) {
            scale = std::max(scale, std::abs(column.solution.column.cost));
        }
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

std::pair<std::vector<ColumnId>, std::vector<double>> ColumnGeneration::nonzero_values() const {
    const std::vector<double> values = column_values();
    std::pair<std::vector<ColumnId>, std::vector<double>> used;
    for (size_t index = 0; index < values.size(); ++index) {
        if (values[index] != 0.0) {
            used.first.push_back(master_.columns()[index].id);
            used.second.push_back(values[index]);
        }
    }
    return used;
}

bool ColumnGeneration::uses_artificial_columns() const {
    const std::span<const ColumnId> ids = master_.artificial_column_ids();
    std::vector<double> values(ids.size());
    master_.solver().primal(ids, values);

    const double tolerance = params_.tolerances->feasibility(context_);
    return std::ranges::any_of(values, [tolerance](double value) { return value > tolerance; });
}

}  // namespace rcspp::column_generation
