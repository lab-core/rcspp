// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <iostream>
#include <memory>
#include <utility>

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

        virtual void extend(const Resource<ResourceType>& resource,
                            const Extender<ResourceType>& extender,
                            Resource<ResourceType>* extended_resource) = 0;

        virtual void extend_back(const Resource<ResourceType>& resource,
                                 const Extender<ResourceType>& extender,
                                 Resource<ResourceType>* extended_resource) {
            extend(resource, extender, extended_resource);
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
}  // namespace rcspp
