// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/resource/concrete/functions/extension/time_window_extension_function.hpp"

#include <algorithm>
#include <iostream>
#include <map>

#include "rcspp/graph/arc.hpp"
#include "rcspp/resource/concrete/functions/dominance/real_value_dominance_function.hpp"

namespace rcspp {

TimeWindowExtensionFunction::TimeWindowExtensionFunction(
    const std::map<size_t, double>& min_time_window_by_arc_id)
    : min_time_window_by_arc_id_(min_time_window_by_arc_id) {}

void TimeWindowExtensionFunction::extend(const Resource<RealResource>& resource,
                                         const Extender<RealResource>& extender,
                                         Resource<RealResource>* extended_resource) {
    double sum_value = resource.get_value() + extender.get_value();

    sum_value = std::max(min_time_window_, sum_value);

    extended_resource->set_value(sum_value);
}

void TimeWindowExtensionFunction::preprocess() {
    min_time_window_ = min_time_window_by_arc_id_.at(arc_id_);
}
}  // namespace rcspp
