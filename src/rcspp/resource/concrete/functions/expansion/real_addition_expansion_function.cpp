// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/resource/concrete/functions/expansion/real_addition_expansion_function.hpp"

void RealAdditionExpansionFunction::expand(const Resource<RealResource>& resource,
                                           const Expander<RealResource>& expander,
                                           Resource<RealResource>& expanded_resource) {
    auto sum_value = resource.get_value() + expander.get_value();

    expanded_resource.set_value(sum_value);
}
