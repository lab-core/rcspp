// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/resource/concrete/functions/feasibility/min_max_feasibility_function.hpp"

#include "rcspp/resource/base/resource.hpp"

namespace rcspp {

auto MinMaxFeasibilityFunction::is_feasible(const Resource<RealResource>& resource) -> bool {
  bool feasible = true;

  if ((resource.get_value() < min_) || (resource.get_value() > max_)) {
    feasible = false;
  }

  return feasible;
}
}  // namespace rcspp
