// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <string>
#include <vector>

namespace rcspp {

/// @brief What the model checks found: one line per problem, empty when the model passes.
struct ModelReport {
        /// @brief One line per problem, naming the component or the arc and the fix.
        std::vector<std::string> problems;

        /// @brief Whether no check found a problem.
        [[nodiscard]] bool ok() const { return problems.empty(); }
};

}  // namespace rcspp
