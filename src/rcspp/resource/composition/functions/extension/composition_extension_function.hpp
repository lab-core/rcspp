// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/extender_composition.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename... ResourceTypes>
class CompositionExtensionFunction
    : public Clonable<CompositionExtensionFunction<ResourceTypes...>,
                      ExtensionFunction<ResourceTypeComposition<ResourceTypes...>>> {
        using ResourceType = ResourceTypeComposition<ResourceTypes...>;

    public:
        void extend(const Resource<ResourceType>& resource, const Extender<ResourceType>& extender,
                    Resource<ResourceType>* extended_resource) override {
            extended_resource->for_each_component(
                resource,
                extender,
                [](auto& ext_res, const auto& res, const auto& exp) { exp.extend(res, &ext_res); });
        }

        void extend_back(const Resource<ResourceType>& resource,
                         const Extender<ResourceType>& extender,
                         Resource<ResourceType>* extended_resource) override {
            extended_resource->for_each_component(
                resource,
                extender,
                [](auto& ext_res, const auto& res, const auto& exp) {
                    exp.extend_back(res, &ext_res);
                });
        }
};
}  // namespace rcspp
