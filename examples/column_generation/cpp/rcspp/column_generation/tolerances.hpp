// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

namespace rcspp::column_generation {

/// @brief What a tolerance may depend on.
struct ToleranceContext {
        /// @brief Typical magnitude of the column costs (at least 1).
        double cost_scale = 1.0;
};

/// @brief The tolerances of a column generation run, as a swappable component.
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
        explicit AbsoluteTolerances(double reduced_cost = 1e-9, double feasibility = 1e-6,
                                    double optimality_gap = 1e-6);

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
        explicit RelativeTolerances(double reduced_cost = 1e-9, double feasibility = 1e-6,
                                    double optimality_gap = 1e-6);

        [[nodiscard]] double reduced_cost(const ToleranceContext& context) const override;
        [[nodiscard]] double feasibility(const ToleranceContext& context) const override;
        [[nodiscard]] double optimality_gap(const ToleranceContext& context) const override;

    private:
        double reduced_cost_;
        double feasibility_;
        double optimality_gap_;
};

}  // namespace rcspp::column_generation
