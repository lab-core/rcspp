// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/extender_composition.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
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
            post_extend(resource, extender, extended_resource);
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
            post_extend_back(resource, extender, extended_resource);
        }

    protected:
        /// @brief Hook fired after the per-component forward extension completes.
        ///
        /// The default is a no-op.  Subclasses override this to apply
        /// composition-level post-processing that needs access to the whole
        /// extended resource and the arc extender (e.g. cross-resource updates).
        ///
        /// @param resource The parent (pre-extension) composite resource.
        /// @param extender The arc extender that was applied.
        /// @param extended_resource The just-extended composite resource to adjust.
        virtual void post_extend(const Resource<ResourceType>& /*resource*/,
                                 const Extender<ResourceType>& /*extender*/,
                                 Resource<ResourceType>* /*extended_resource*/) {}

        /// @brief Backward counterpart of @ref post_extend; defaults to it.
        virtual void post_extend_back(const Resource<ResourceType>& resource,
                                      const Extender<ResourceType>& extender,
                                      Resource<ResourceType>* extended_resource) {
            post_extend(resource, extender, extended_resource);
        }
};
}  // namespace rcspp
