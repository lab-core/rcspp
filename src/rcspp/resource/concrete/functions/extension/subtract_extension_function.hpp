// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename ResourceType>
class SubtractExtensionFunction
    : public Clonable<SubtractExtensionFunction<ResourceType>, ExtensionFunction<ResourceType>> {
    public:
        void extend(const ResourceType& resource, const ResourceType& extender_value,
                    ResourceType* extended_resource) override {
            auto difference = resource.subtract(extender_value.get_value());
            extended_resource->set_value(difference);
        }
};
}  // namespace rcspp
