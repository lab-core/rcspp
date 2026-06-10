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
        void extend(const ContainerResourceType& resource,
                    const ContainerResourceType& extender_value,
                    ContainerResourceType* extended_resource) override {
            auto union_value = resource.get_union(extender_value.get_value());
            extended_resource->set_value(union_value);
        }
};
}  // namespace rcspp
