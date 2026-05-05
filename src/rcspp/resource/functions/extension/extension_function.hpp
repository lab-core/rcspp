// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <iostream>
#include <memory>
#include <utility>

#include "rcspp/resource/composition/resource_value_composition.hpp"

namespace rcspp {

template <typename ResourceType>
class Resource;

template <typename ResourceType>
class Extender;

template <typename ResourceType>
class Arc;

template <typename ResourceType>
class ExtensionFunction {
    public:
        virtual ~ExtensionFunction() = default;

        virtual void extend(const ResourceType& resource, const ResourceType& extender_value,
                            ResourceType* extended_resource) = 0;

        virtual void extend_back(const ResourceType& resource, const ResourceType& extender_value,
                                 ResourceType* extended_resource) {
            extend(resource, extender_value, extended_resource);
        }

        [[nodiscard]] virtual auto clone() const -> std::unique_ptr<ExtensionFunction> = 0;

        template <typename GraphResourceType>
        auto create(const Arc<GraphResourceType>& arc) -> std::unique_ptr<ExtensionFunction> {
            auto new_extension_function = clone();
            new_extension_function->preprocess(arc.origin->id, arc.destination->id);
            return new_extension_function;
        }

    protected:
        virtual void preprocess(size_t origin_id, size_t destination_id) {}
};

// Specialization for ResourceBaseComposition: extension functions receive the full Resource object.
template <typename... ResourceTypes>
class ExtensionFunction<ResourceValueComposition<ResourceTypes...>> {
    public:
        virtual ~ExtensionFunction() = default;

        virtual void extend(
            const Resource<ResourceValueComposition<ResourceTypes...>>& resource,
            const Extender<ResourceValueComposition<ResourceTypes...>>& extender,
            Resource<ResourceValueComposition<ResourceTypes...>>* extended_resource) = 0;

        virtual void extend_back(
            const Resource<ResourceValueComposition<ResourceTypes...>>& resource,
            const Extender<ResourceValueComposition<ResourceTypes...>>& extender,
            Resource<ResourceValueComposition<ResourceTypes...>>* extended_resource) {
            extend(resource, extender, extended_resource);
        }

        [[nodiscard]] virtual auto clone() const
            -> std::unique_ptr<ExtensionFunction<ResourceValueComposition<ResourceTypes...>>> = 0;

        template <typename GraphResourceType>
        auto create(const Arc<GraphResourceType>& arc)
            -> std::unique_ptr<ExtensionFunction<ResourceValueComposition<ResourceTypes...>>> {
            auto new_extension_function = clone();
            new_extension_function->preprocess(arc.origin->id, arc.destination->id);
            return new_extension_function;
        }

    protected:
        virtual void preprocess(size_t origin_id, size_t destination_id) {}
};

}  // namespace rcspp
