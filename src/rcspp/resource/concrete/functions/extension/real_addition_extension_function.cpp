// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/resource/concrete/functions/extension/real_addition_extension_function.hpp"

namespace rcspp {

void RealAdditionExpansionFunction::extend(const Resource<RealResource>& resource,
                                           const Extender<RealResource>& extender,
                                           Resource<RealResource>* extended_resource) {
    auto sum_value = resource.get_value() + extender.get_value();

    extended_resource->set_value(sum_value);
}
}  // namespace rcspp
