// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/resource/concrete/functions/expansion/time_window_expansion_function.hpp"

#include <algorithm>
#include <iostream>
#include <map>

#include "rcspp/graph/arc.hpp"
#include "rcspp/resource/concrete/functions/dominance/real_value_dominance_function.hpp"

namespace rcspp {

TimeWindowExpansionFunction::TimeWindowExpansionFunction(
    const std::map<size_t, double>& min_time_window_by_arc_id)
    : min_time_window_by_arc_id_(min_time_window_by_arc_id) {}

void TimeWindowExpansionFunction::expand(const Resource<RealResource>& resource,
                                         const Expander<RealResource>& expander,
                                         Resource<RealResource>& expanded_resource) {
    double sum_value = resource.get_value() + expander.get_value();

    sum_value = std::max(min_time_window_, sum_value);

    expanded_resource.set_value(sum_value);
}

void TimeWindowExpansionFunction::preprocess() {
    min_time_window_ = min_time_window_by_arc_id_.at(arc_id_);
}
}  // namespace rcspp
