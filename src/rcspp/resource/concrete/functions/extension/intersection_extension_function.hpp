// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename ContainerResourceType>
class IntersectionExtensionFunction
    : public Clonable<IntersectionExtensionFunction<ContainerResourceType>,
                      ExtensionFunction<ContainerResourceType>> {
    public:
        void extend(const Resource<ContainerResourceType>& resource,
                    const Extender<ContainerResourceType>& extender,
                    Resource<ContainerResourceType>* extended_resource) override {
            auto intersection_container = resource.get_intersection(extender.get_value());
            extended_resource->set_value(intersection_container);
        }
};
}  // namespace rcspp
