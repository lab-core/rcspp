// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <vector>

namespace rcspp {

struct Row {
        size_t index;
        long double coefficient;
};

// Represents a column in the LP master problem.
// cost: sum of original arc costs along the path (no dual contribution).
// rows: aggregated constraint coefficients (Row.coefficient summed per Row.index).
struct Column {
        double cost = 0.0;
        std::vector<Row> rows;
};

}  // namespace rcspp
