// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename ContainerResourceType>
class UnionExtensionFunction : public Clonable<UnionExtensionFunction<ContainerResourceType>,
                                               ExtensionFunction<ContainerResourceType>> {
    public:
        void extend(const Resource<ContainerResourceType>& resource,
                    const Extender<ContainerResourceType>& extender,
                    Resource<ContainerResourceType>* extended_resource) override {
            auto union_container = resource.get_union(extender.get_value());
            extended_resource->set_value(union_container);
        }
};
}  // namespace rcspp
