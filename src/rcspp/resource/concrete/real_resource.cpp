// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/resource/concrete/real_resource.hpp"

#include "functions/cost/real_value_cost_function.hpp"
#include "functions/dominance/real_value_dominance_function.hpp"
#include "functions/extension/real_addition_extension_function.hpp"
#include "functions/feasibility/min_max_feasibility_function.hpp"
#include "rcspp/resource/base/resource_base.hpp"
#include "rcspp/resource/functions/feasibility/trivial_feasibility_function.hpp"

namespace rcspp {

RealResource::RealResource() : value_(0) {}

RealResource::RealResource(double value) : value_(value) {}

auto RealResource::get_value() const -> double {
    return value_;
}

void RealResource::set_value(double value) {
    value_ = value;
}

void RealResource::reset() {
    value_ = 0;
}
}  // namespace rcspp
