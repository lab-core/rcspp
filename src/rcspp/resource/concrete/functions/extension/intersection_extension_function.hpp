// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename ContainerResourceType>
class IntersectionExtensionFunction
    : public Clonable<IntersectionExtensionFunction<ContainerResourceType>,
                      ExtensionFunction<ContainerResourceType>> {
    public:
        void extend(const ContainerResourceType& resource,
                    const ContainerResourceType& extender_value,
                    ContainerResourceType* extended_resource) override {
            auto intersection_value = resource.get_intersection(extender_value.get_value());
            extended_resource->set_value(intersection_value);
        }
};
}  // namespace rcspp
