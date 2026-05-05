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
            extended_resource->apply(
                resource,
                extender,
                [](auto& ext_res_vec, const auto& res_vec, const auto& exp_vec) {
                    for (size_t i = 0; i < exp_vec.size(); ++i) {
                        exp_vec[i]->extend(*res_vec[i], ext_res_vec[i].get());
                    }
                });
        }

        void extend_back(const Resource<ResourceType>& resource,
                         const Extender<ResourceType>& extender,
                         Resource<ResourceType>* extended_resource) override {
            extended_resource->apply(
                resource,
                extender,
                [](auto& ext_res_vec, const auto& res_vec, const auto& exp_vec) {
                    for (size_t i = 0; i < exp_vec.size(); ++i) {
                        exp_vec[i]->extend_back(*res_vec[i], ext_res_vec[i].get());
                    }
                });
        }
};
}  // namespace rcspp
