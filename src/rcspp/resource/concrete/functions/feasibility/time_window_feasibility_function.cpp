// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/resource/concrete/functions/feasibility/time_window_feasibility_function.hpp"

#include <iostream>
#include <limits>
#include <map>

#include "rcspp/resource/base/resource.hpp"

namespace rcspp {

TimeWindowFeasibilityFunction::TimeWindowFeasibilityFunction(
    const std::map<size_t, double>& max_time_window_by_node_id)
    : max_time_window_by_node_id_(max_time_window_by_node_id),
      max_time_window_(std::numeric_limits<double>::infinity()) {}

auto TimeWindowFeasibilityFunction::is_feasible(const Resource<RealResource>& resource) -> bool {
    bool feasible = true;

    if (resource.get_value() > max_time_window_) {
        feasible = false;
    }

    return feasible;
}

void TimeWindowFeasibilityFunction::preprocess() {
    max_time_window_ = max_time_window_by_node_id_.at(node_id_);
}
}  // namespace rcspp