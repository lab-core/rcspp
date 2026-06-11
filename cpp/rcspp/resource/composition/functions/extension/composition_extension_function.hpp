// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/extender_composition.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

/// @brief Extension function that propagates a composed resource by extending each component.
///
/// Both forward (`extend`) and backward (`extend_back`) extension iterate over all
/// constituent sub-resource components and delegate to the corresponding per-component
/// extender.
///
/// @tparam ResourceTypes The individual resource types forming the composition.
template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class CompositionExtensionFunction
    : public Clonable<CompositionExtensionFunction<ResourceTypes...>,
                      ExtensionFunction<ResourceTypeComposition<ResourceTypes...>>> {
        using ResourceType = ResourceTypeComposition<ResourceTypes...>;

    public:
        /// @brief Extends @p resource in the forward direction using @p extender.
        ///
        /// For each component triple `(ext_res, res, exp)` from @p extended_resource,
        /// @p resource, and @p extender respectively, calls `exp.extend(res, &ext_res)`.
        ///
        /// @param resource          The current composed resource state.
        /// @param extender          The composed extender describing arc consumption.
        /// @param extended_resource Output pointer to the resulting composed resource.
        void extend(const Resource<ResourceType>& resource, const Extender<ResourceType>& extender,
                    Resource<ResourceType>* extended_resource) override {
            extended_resource->for_each_component(
                resource,
                extender,
                [](auto& ext_res, const auto& res, const auto& exp) { exp.extend(res, &ext_res); });
        }

        /// @brief Extends @p resource in the backward direction using @p extender.
        ///
        /// For each component triple `(ext_res, res, exp)` calls `exp.extend_back(res, &ext_res)`.
        ///
        /// @param resource          The current composed resource state.
        /// @param extender          The composed extender describing arc consumption.
        /// @param extended_resource Output pointer to the resulting composed resource.
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
