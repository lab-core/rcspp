// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/column_generation/master_initialization.hpp"

#include <cstddef>
#include <vector>

namespace rcspp::column_generation {

void ArtificialColumns::initialize(MasterProblem& master, const ToleranceContext& context) {
    const size_t row_count = master.num_rows();
    std::vector<double> lower(row_count);
    std::vector<double> upper(row_count);
    master.backend().row_bounds(lower, upper);

    // A row that x = 0 violates gets one column with the sign that satisfies it alone.
    std::vector<size_t> rows;
    std::vector<double> coefficients;
    for (size_t row = 0; row < row_count; ++row) {
        if (lower[row] > 0.0) {
            rows.push_back(row);
            coefficients.push_back(1.0);
        } else if (upper[row] < 0.0) {
            rows.push_back(row);
            coefficients.push_back(-1.0);
        }
    }

    if (!rows.empty()) {
        master.add_artificial_columns(rows, coefficients, cost(context));
    }
}

double ArtificialColumns::cost(const ToleranceContext& context) const {
    return cost_ > 0.0 ? cost_ : automatic_cost_factor * context.cost_scale;
}

void NoInitialization::initialize(MasterProblem& /*master*/, const ToleranceContext& /*context*/) {}

}  // namespace rcspp::column_generation
