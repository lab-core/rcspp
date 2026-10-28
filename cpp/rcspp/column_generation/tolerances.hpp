// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

namespace rcspp::column_generation {

/// @brief Default reduced-cost tolerance: a column improves when its reduced cost is below minus
///        this.
inline constexpr double default_reduced_cost_tolerance = 1e-9;

/// @brief Default feasibility tolerance: smaller values count as zero.
inline constexpr double default_feasibility_tolerance = 1e-6;

/// @brief Default gap between the bound and the LP value at which the loop may stop early.
inline constexpr double default_optimality_gap = 1e-6;

/// @brief What a tolerance may depend on.
struct ToleranceContext {
        /// @brief Typical magnitude of the column costs (at least 1).
        double cost_scale = 1.0;
};

/// @brief The tolerances of a column generation run, as a swappable component.
///
/// The loop never uses a reduced-cost tolerance below the master solver's own
/// (@c MasterSolver::dual_tolerance): the LP solver would leave a column improving by less out of
/// its basis, and pricing would return it again and again. LP solvers apply their tolerance to a
/// scaled model, so this is a strong guard, not a guarantee: for costs far from 1, use
/// @c RelativeTolerances or a larger tolerance.
class Tolerances {
    public:
        Tolerances() = default;
        Tolerances(const Tolerances&) = default;
        Tolerances& operator=(const Tolerances&) = default;
        Tolerances(Tolerances&&) = default;
        Tolerances& operator=(Tolerances&&) = default;
        virtual ~Tolerances() = default;

        /// @brief A column improves the master when its reduced cost is below minus this value.
        [[nodiscard]] virtual double reduced_cost(const ToleranceContext& context) const = 0;

        /// @brief Values below this are zero (artificial columns, integrality).
        [[nodiscard]] virtual double feasibility(const ToleranceContext& context) const = 0;

        /// @brief Gap between the bound and the LP value at which the loop may stop early.
        [[nodiscard]] virtual double optimality_gap(const ToleranceContext& context) const = 0;
};

/// @brief Fixed tolerances, whatever the scale of the costs (the default).
class AbsoluteTolerances final : public Tolerances {
    public:
        /// @brief Constructs the tolerances.
        ///
        /// @param reduced_cost   See @c Tolerances::reduced_cost.
        /// @param feasibility    See @c Tolerances::feasibility.
        /// @param optimality_gap See @c Tolerances::optimality_gap.
        explicit AbsoluteTolerances(double reduced_cost = default_reduced_cost_tolerance,
                                    double feasibility = default_feasibility_tolerance,
                                    double optimality_gap = default_optimality_gap);

        [[nodiscard]] double reduced_cost(const ToleranceContext& context) const override;
        [[nodiscard]] double feasibility(const ToleranceContext& context) const override;
        [[nodiscard]] double optimality_gap(const ToleranceContext& context) const override;

    private:
        double reduced_cost_;
        double feasibility_;
        double optimality_gap_;
};

/// @brief Tolerances on costs scaled by @c ToleranceContext::cost_scale.
///
/// The reduced-cost and gap tolerances are multiplied by the cost scale; the feasibility
/// tolerance applies to column values, not costs, so it is not scaled.
class RelativeTolerances final : public Tolerances {
    public:
        /// @brief Constructs the tolerances.
        ///
        /// @param reduced_cost   Reduced-cost tolerance per unit of cost scale.
        /// @param feasibility    See @c Tolerances::feasibility.
        /// @param optimality_gap Gap tolerance per unit of cost scale.
        explicit RelativeTolerances(double reduced_cost = default_reduced_cost_tolerance,
                                    double feasibility = default_feasibility_tolerance,
                                    double optimality_gap = default_optimality_gap);

        [[nodiscard]] double reduced_cost(const ToleranceContext& context) const override;
        [[nodiscard]] double feasibility(const ToleranceContext& context) const override;
        [[nodiscard]] double optimality_gap(const ToleranceContext& context) const override;

    private:
        double reduced_cost_;
        double feasibility_;
        double optimality_gap_;
};

}  // namespace rcspp::column_generation
