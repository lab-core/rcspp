// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename ResourceType>
class IntersectionExtensionFunction : public Clonable<IntersectionExtensionFunction<ResourceType>,
                                                      ExtensionFunction<ResourceType>> {
    public:
        void extend(const Resource<ResourceType>& resource, const Extender<ResourceType>& extender,
                    Resource<ResourceType>* extended_resource) override {
            auto intersection_container = resource.get_intersection(extender.get_value());
            extended_resource->set_value(intersection_container);
        }
};
}  // namespace rcspp
