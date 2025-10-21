// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/resource/concrete/functions/cost/real_value_cost_function.hpp"

#include <iostream>

#include "rcspp/resource/base/resource.hpp"

namespace rcspp {

auto RealValueCostFunction::get_cost(const Resource<RealResource>& real_resource) const -> double {
    return real_resource.get_value();
}
}  // namespace rcspp