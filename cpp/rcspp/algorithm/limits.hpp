// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <limits>

namespace rcspp {

constexpr size_t kNoLimit = std::numeric_limits<int>::max() / 2;  // to avoid overflow

}  // namespace rcspp
