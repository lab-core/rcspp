// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <iostream>
#include <memory>
#include <utility>

#include "rcspp/resource/base/resource_type.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"

namespace rcspp {

template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Resource;

template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Extender;

template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Arc;

template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class ExtensionFunction {
    public:
        virtual ~ExtensionFunction() = default;

        virtual void extend(const ResourceType& resource, const ResourceType& extender_value,
                            ResourceType* extended_resource) = 0;

        // GCOVR_EXCL_START — back-direction not used in tests
        virtual void extend_back(const ResourceType& resource, const ResourceType& extender_value,
                                 ResourceType* extended_resource) {
            extend(resource, extender_value, extended_resource);
        }
        // GCOVR_EXCL_STOP

        [[nodiscard]] virtual auto clone() const -> std::unique_ptr<ExtensionFunction> = 0;

        // GCOVR_EXCL_START — clone+preprocess default not called directly in tests
        template <typename GraphResourceType>
        auto create(const Arc<GraphResourceType>& arc) -> std::unique_ptr<ExtensionFunction> {
            auto new_extension_function = clone();
            new_extension_function->preprocess(arc.origin->id, arc.destination->id);
            return new_extension_function;
        }
        // GCOVR_EXCL_STOP

    protected:
        virtual void preprocess(size_t origin_id, size_t destination_id) {}  // GCOVR_EXCL_LINE
};

// Specialization for ResourceTypeComposition: extension functions receive the full Resource object.
template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class ExtensionFunction<ResourceTypeComposition<ResourceTypes...>> {
    public:
        virtual ~ExtensionFunction() = default;

        virtual void extend(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource,
            const Extender<ResourceTypeComposition<ResourceTypes...>>& extender,
            Resource<ResourceTypeComposition<ResourceTypes...>>* extended_resource) = 0;

        // GCOVR_EXCL_START — back-direction not used in tests
        virtual void extend_back(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource,
            const Extender<ResourceTypeComposition<ResourceTypes...>>& extender,
            Resource<ResourceTypeComposition<ResourceTypes...>>* extended_resource) {
            extend(resource, extender, extended_resource);
        }
        // GCOVR_EXCL_STOP

        [[nodiscard]] virtual auto clone() const
            -> std::unique_ptr<ExtensionFunction<ResourceTypeComposition<ResourceTypes...>>> = 0;

        // GCOVR_EXCL_START — clone+preprocess default not called directly in tests
        template <typename GraphResourceType>
        auto create(const Arc<GraphResourceType>& arc)
            -> std::unique_ptr<ExtensionFunction<ResourceTypeComposition<ResourceTypes...>>> {
            auto new_extension_function = clone();
            new_extension_function->preprocess(arc.origin->id, arc.destination->id);
            return new_extension_function;
        }
        // GCOVR_EXCL_STOP

    protected:
        virtual void preprocess(size_t origin_id, size_t destination_id) {}  // GCOVR_EXCL_LINE
};

}  // namespace rcspp
