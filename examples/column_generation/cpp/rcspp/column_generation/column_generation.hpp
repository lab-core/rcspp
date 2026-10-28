// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <span>
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
        /// @brief Called after every iteration; read-only.
        std::function<void(const IterationInfo&)> on_iteration;
        /// @brief Called after each proven convergence; may change a pricer and ask to continue.
        std::function<ConvergenceAction(ConvergenceContext&)> on_converged;
};

/// @brief How a run ended.
enum class ColumnGenerationStatus {
    Optimal,     ///< proven: no improving column exists, and no artificial column is used
    NotProven,   ///< no new column, but some pricer did not prove it found the best ones
    Infeasible,  ///< converged with an artificial column still in use: no solution exists
    /// The master LP, with the columns it has so far, has no feasible solution. This does not
    /// prove the problem infeasible. Add starting columns, or use @c ArtificialColumns.
    RestrictedMasterInfeasible,
    /// The master LP is unbounded: some column can be used without limit at a negative cost.
    /// More columns cannot bound it, so the problem itself is unbounded; look for a missing
    /// convexity row or bound.
    Unbounded,
    TimeLimit,       ///< stopped by @c ColumnGenerationParams::timeout_s, in LP or pricing
    IterationLimit,  ///< stopped by @c ColumnGenerationParams::max_iterations
    SolverError,     ///< an LP solve failed for another reason (numerical trouble, interruption…)
};

/// @brief Result of a run.
struct ColumnGenerationResult {
        /// @brief How the run ended.
        ColumnGenerationStatus status = ColumnGenerationStatus::SolverError;
        /// @brief Value of the last master LP solved.
        double objective = std::numeric_limits<double>::infinity();
        /// @brief A proven lower bound on the master LP optimum (`−∞` when none).
        double lower_bound = -std::numeric_limits<double>::infinity();
        /// @brief Number of iterations (LP solves).
        size_t iterations = 0;
        /// @brief Counters and times.
        Statistics statistics;
};

/// @brief The column generation loop: solve the master LP, price its duals, add columns.
///
/// Holds the master by reference: the caller keeps it alive. @c run() may be called again, e.g.
/// after the master or a pricer changed; the master is initialized only by the first call.
class ColumnGeneration {
    public:
        /// @brief Constructs the loop.
        ///
        /// @param master The master, with its rows, pricers and any starting columns.
        /// @param params Options.
        explicit ColumnGeneration(MasterProblem& master, ColumnGenerationParams params = {});

        /// @brief Runs column generation until convergence or a limit.
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
        };

        // Prices every pricer once and adds what they find.
        PricingRound price(std::span<const double> duals, const Timer& total_timer,
                           double reduced_cost_tolerance);

        // At least 1: the largest pricer cost scale and starting column cost.
        [[nodiscard]] ToleranceContext compute_tolerance_context() const;

        [[nodiscard]] std::vector<double> column_values() const;

        [[nodiscard]] bool uses_artificial_columns() const;

        MasterProblem& master_;
        ColumnGenerationParams params_;
        ToleranceContext context_;
        bool initialized_ = false;
};

}  // namespace rcspp::column_generation
