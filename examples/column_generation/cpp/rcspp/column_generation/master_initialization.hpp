// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/column_generation/master_problem.hpp"
#include "rcspp/column_generation/tolerances.hpp"

namespace rcspp::column_generation {

/// @brief Makes the first master LP solvable, before any column is generated.
///
/// A swappable component: @c ArtificialColumns is the default; later alternatives include a
/// two-phase method and Farkas pricing.
class MasterInitialization {
    public:
        MasterInitialization() = default;
        MasterInitialization(const MasterInitialization&) = default;
        MasterInitialization& operator=(const MasterInitialization&) = default;
        MasterInitialization(MasterInitialization&&) = default;
        MasterInitialization& operator=(MasterInitialization&&) = default;
        virtual ~MasterInitialization() = default;

        /// @brief Prepares @p master; called once, before the first LP solve.
        ///
        /// @param master  The master, with its rows, pricers and starting columns.
        /// @param context The cost scale of the run.
        virtual void initialize(MasterProblem& master, const ToleranceContext& context) = 0;
};

/// @brief Single-phase big-M: one costly artificial column per row that `x = 0` violates.
///
/// Each artificial column alone satisfies its row (coefficient +1 when the row's lower bound is
/// positive, −1 when its upper bound is negative). Pricing then brings cheaper real columns that
/// push the artificial ones out. An artificial column still used once column generation has
/// converged means the master is infeasible.
class ArtificialColumns final : public MasterInitialization {
    public:
        /// @brief Factor of the automatic cost: `automatic_cost_factor × cost_scale`.
        static constexpr double automatic_cost_factor = 1e6;

        /// @brief Constructs the initialization.
        ///
        /// @param cost Cost of every artificial column; 0 means automatic.
        explicit ArtificialColumns(double cost = 0.0) : cost_(cost) {}

        void initialize(MasterProblem& master, const ToleranceContext& context) override;

        /// @brief The cost given to artificial columns in a run with @p context.
        [[nodiscard]] double cost(const ToleranceContext& context) const;

    private:
        double cost_;
};

}  // namespace rcspp::column_generation
