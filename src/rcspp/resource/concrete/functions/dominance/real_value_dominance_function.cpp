// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/resource/concrete/functions/dominance/real_value_dominance_function.hpp"

#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/concrete/real_resource.hpp"

namespace rcspp {

auto RealValueDominanceFunction::check_dominance(const Resource<RealResource>& lhs_resource,
                                                 const Resource<RealResource>& rhs_resource)
    -> bool {
    return lhs_resource.get_value() <= rhs_resource.get_value();
}
}  // namespace rcspp
