// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/column_generation/tolerances.hpp"

namespace rcspp::column_generation {

AbsoluteTolerances::AbsoluteTolerances(double reduced_cost, double feasibility,
                                       double optimality_gap)
    : reduced_cost_(reduced_cost), feasibility_(feasibility), optimality_gap_(optimality_gap) {}

double AbsoluteTolerances::reduced_cost(const ToleranceContext& /*context*/) const {
    return reduced_cost_;
}

double AbsoluteTolerances::feasibility(const ToleranceContext& /*context*/) const {
    return feasibility_;
}

double AbsoluteTolerances::optimality_gap(const ToleranceContext& /*context*/) const {
    return optimality_gap_;
}

RelativeTolerances::RelativeTolerances(double reduced_cost, double feasibility,
                                       double optimality_gap)
    : reduced_cost_(reduced_cost), feasibility_(feasibility), optimality_gap_(optimality_gap) {}

double RelativeTolerances::reduced_cost(const ToleranceContext& context) const {
    return reduced_cost_ * context.cost_scale;
}

// Column values, not costs: not scaled.
double RelativeTolerances::feasibility(const ToleranceContext& /*context*/) const {
    return feasibility_;
}

double RelativeTolerances::optimality_gap(const ToleranceContext& context) const {
    return optimality_gap_ * context.cost_scale;
}

}  // namespace rcspp::column_generation
