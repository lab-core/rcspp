// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "rcspp/column_generation/master_initialization.hpp"
#include "rcspp/column_generation/master_problem.hpp"
#include "rcspp/column_generation/master_solver.hpp"
#include "rcspp/column_generation/statistics.hpp"
#include "rcspp/column_generation/tolerances.hpp"
#include "rcspp/column_generation/types.hpp"
#include "rcspp/utils/timer.hpp"

namespace rcspp::column_generation {

/// @brief What the convergence hook decides.
enum class ConvergenceAction {
    Stop,      ///< finish the run
    Continue,  ///< the hook changed something: solve the master and price again
};

/// @brief What the convergence hook sees and may change.
struct ConvergenceContext {
        /// @brief The master: its columns and pricers (e.g. to call @c Pricer::update).
        MasterProblem& master;
        /// @brief Iterations so far.
        size_t iteration;
        /// @brief Value of the converged master LP.
        double objective;
        /// @brief Its duals, one per row.
        std::span<const double> duals;
        /// @brief Value of every column, indexed like @c MasterProblem::columns().
        std::span<const double> values;
};

/// @brief Options of a column generation run.
struct ColumnGenerationParams {
        /// @brief Maximum number of iterations (LP solves).
        size_t max_iterations = std::numeric_limits<size_t>::max();
        /// @brief Wall-clock limit of the run, in seconds.
        double timeout_s = std::numeric_limits<double>::infinity();
        /// @brief Options of every LP solve; its time limit is capped by what is left of
        ///        @c timeout_s.
        RelaxationParams relaxation;
        /// @brief Tolerances; null means @c AbsoluteTolerances with its defaults.
        std::shared_ptr<const Tolerances> tolerances;
        /// @brief How the first LP becomes solvable; null means @c ArtificialColumns.
        std::shared_ptr<MasterInitialization> initialization;
        /// @brief Which statistics to collect.
        StatisticsParams statistics;
        /// @brief Stop (@c AbsoluteGapLimit) when the LP value minus the lower bound
        ///        (@c IterationInfo::lower_bound) is at most this; 0 is off.
        double absolute_gap = 0.0;
        /// @brief Stop (@c RelativeGapLimit) when the LP value minus the lower bound is at most
        ///        this times the LP value's magnitude; 0 is off.
        double relative_gap = 0.0;
        /// @brief The cost of a solution already known: stop (@c Cutoff) when the lower bound
        ///        reaches it, as nothing here costs less; infinity is off.
        double cutoff = std::numeric_limits<double>::infinity();
        /// @brief Every solution has an integer cost: the lower bound is rounded up, for the
        ///        cutoff and for a stop (@c IntegerObjectiveGap) when it reaches the LP value
        ///        rounded up.
        ///
        /// The stops are checked after an iteration that added columns and has a lower bound, in
        /// this order: the cutoff, the absolute gap, the relative gap, the integer objective; the
        /// first one reached gives the status. They compare with the reduced-cost tolerance's
        /// margin.
        bool integer_objective = false;
        /// @brief Log a line every this many iterations, the last one too, and one when the run
        ///        ends, through the engine's logger at level INFO; 0 logs nothing.
        size_t log_frequency = 0;
        /// @brief Called after every iteration; read-only.
        std::function<void(const IterationInfo&)> on_iteration;
        /// @brief Called after each proven convergence; may change a pricer and ask to continue.
        std::function<ConvergenceAction(ConvergenceContext&)> on_converged;
};

/// @brief How a run ended.
enum class ColumnGenerationStatus {
    Optimal,    ///< proven: no improving column exists, and no artificial column is used
    NotProven,  ///< no new column, but some pricer did not prove it found the best ones
    /// Converged with an artificial column still in use: no solution exists at the artificial
    /// cost used (@c Statistics::artificial_cost). A solution whose columns cost more than that
    /// per row could still exist: see @c ArtificialColumns.
    Infeasible,
    /// The master LP, with the columns it has so far, has no feasible solution. This does not
    /// prove the problem infeasible. Add starting columns, or use @c ArtificialColumns.
    RestrictedMasterInfeasible,
    /// The master LP is unbounded: some column can be used without limit at a negative cost.
    /// More columns cannot bound it, so the problem itself is unbounded; look for a missing
    /// convexity row or bound.
    Unbounded,
    TimeLimit,       ///< stopped by @c ColumnGenerationParams::timeout_s, in LP or pricing
    IterationLimit,  ///< stopped by @c ColumnGenerationParams::max_iterations
    /// Stopped by @c ColumnGenerationParams::cutoff: the lower bound reached the cutoff, so no
    /// solution here costs less than it. Checked first of the early stops.
    Cutoff,
    /// Stopped by @c ColumnGenerationParams::absolute_gap: the LP value minus the lower bound is
    /// at most the gap. Checked after @c Cutoff.
    AbsoluteGapLimit,
    /// Stopped by @c ColumnGenerationParams::relative_gap: the LP value minus the lower bound is
    /// at most the gap times the LP value's magnitude. Checked after @c AbsoluteGapLimit.
    RelativeGapLimit,
    /// Stopped by @c ColumnGenerationParams::integer_objective: the lower bound rounded up
    /// reached the LP value rounded up, so more iterations cannot raise what it proves about an
    /// integer solution. Checked last.
    IntegerObjectiveGap,
    SolverError,  ///< an LP solve failed for another reason (numerical trouble, interruption…)
};

/// @brief Result of a run.
struct ColumnGenerationResult {
        /// @brief How the run ended.
        ColumnGenerationStatus status = ColumnGenerationStatus::SolverError;
        /// @brief Value of the last master LP solved.
        double objective = std::numeric_limits<double>::infinity();
        /// @brief A proven lower bound on the master LP optimum: the objective when @c Optimal,
        ///        else the best Lagrangian bound of the run's iterations (-infinity when none is
        ///        known; see @c IterationInfo::lower_bound).
        double lower_bound = -std::numeric_limits<double>::infinity();
        /// @brief Number of iterations (LP solves).
        size_t iterations = 0;
        /// @brief Counters and times.
        Statistics statistics;
};

/// @brief The column generation loop: solve the master LP, price its duals, add columns.
///
/// Holds the master by reference: the caller keeps it alive. @c run() may be called again, e.g.
/// after the master or a pricer changed. The master is initialized once, by the first run of any
/// loop over it.
class ColumnGeneration {
    public:
        /// @brief Constructs the loop.
        ///
        /// @param master The master, with its rows, pricers and any starting columns.
        /// @param params Options.
        /// @throws std::invalid_argument When a gap is negative or not a number, or the search
        ///                               statistics are asked without the pricers'.
        explicit ColumnGeneration(MasterProblem& master, ColumnGenerationParams params = {});

        /// @brief Runs column generation until convergence or a limit, then has the master
        ///        solver restore the user's model (@c MasterSolver::restore_user_model), also when
        ///        the run ends with an exception.
        [[nodiscard]] ColumnGenerationResult run();

        /// @brief Solves the master as a MIP over the columns it holds (a heuristic: the best
        ///        plan may need columns that column generation never produced).
        ///
        /// @param params Time limit, gap and column type.
        /// @return The master solver's status.
        SolverStatus solve_restricted_master_integer(const IntegerParams& params) {
            return master_.solver().solve_integer(params);
        }

        /// @brief The options, with the default components filled in.
        [[nodiscard]] const ColumnGenerationParams& params() const { return params_; }

        /// @brief The cost scale and other inputs of the tolerances, set by the first run.
        [[nodiscard]] const ToleranceContext& tolerance_context() const { return context_; }

    private:
        struct PricingRound {
                size_t new_columns;
                bool proven;
                // What the pricers add to the LP value to give the Lagrangian bound (-infinity
                // when unknown).
                double bound_terms;
                // What each pricer did, when the run collects it.
                std::optional<std::vector<PricerIterationInfo>> pricers;
        };

        // The loop itself: solve, price, add columns, until convergence or a limit.
        [[nodiscard]] ColumnGenerationResult iterate();

        // On this loop's first run: sets the tolerances' inputs, and initializes the master unless
        // another loop did.
        void initialize();

        // Prices every pricer once and adds what they find.
        PricingRound price(std::span<const double> duals, const Timer& total_timer,
                           double reduced_cost_tolerance, std::span<const double> paths_allowed,
                           bool wants_info);

        // How many paths each pricer's convexity row allows: its upper bound, infinity without a
        // row. Read once per run and after a restart, as a master solver may copy its model to
        // answer.
        [[nodiscard]] std::vector<double> paths_allowed() const;

        // The status of a stop on the cutoff or the gap, or none to go on.
        [[nodiscard]] std::optional<ColumnGenerationStatus> early_stop(double objective,
                                                                       double bound,
                                                                       double tolerance) const;

        // At least 1: the largest pricer cost scale, starting column cost and cost of the user's
        // own variables.
        [[nodiscard]] ToleranceContext compute_tolerance_context() const;

        [[nodiscard]] std::vector<double> column_values() const;

        // The ids and values of the columns the last LP solution uses.
        [[nodiscard]] std::pair<std::vector<ColumnId>, std::vector<double>> nonzero_values() const;

        [[nodiscard]] bool uses_artificial_columns() const;

        MasterProblem& master_;
        ColumnGenerationParams params_;
        ToleranceContext context_;
        bool initialized_ = false;
};

}  // namespace rcspp::column_generation
