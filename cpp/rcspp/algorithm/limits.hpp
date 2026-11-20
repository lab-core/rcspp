// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <limits>

namespace rcspp {

/// @brief "No limit", for every count limit of the params (@c max_solutions,
///        @c max_iterations, @c max_extended_labels_per_node, @c max_admitted_labels_per_node,
///        ...): a large finite value, so that adding to it cannot overflow. A limit at or above it
///        is no limit.
constexpr size_t kNoLimit = std::numeric_limits<int>::max() / 2;

}  // namespace rcspp
