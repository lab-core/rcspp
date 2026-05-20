// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <utility>

#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/composition/composition.hpp"

namespace rcspp {

// Specialization for ResourceComposition
template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class Extender<ResourceTypeComposition<ResourceTypes...>>
    : public ExtenderPrototype<Extender<ResourceTypeComposition<ResourceTypes...>>,
                               ResourceTypeComposition<ResourceTypes...>>,
      public Composition<Extender, ResourceTypes...> {
        using ResourceType = ResourceTypeComposition<ResourceTypes...>;
        using Prototype = ExtenderPrototype<Extender, ResourceType>;

    public:
        Extender() = default;

        Extender(std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                 const size_t arc_id)
            : Prototype(std::move(extension_function), arc_id) {}

        [[nodiscard]] auto clone(const Arc<ResourceType>& arc) const -> auto {
            auto new_extender =
                std::make_unique<Extender>(this->extension_function_->create(arc), arc.id);
            this->apply(*new_extender, [&arc](const auto& extenders, auto& new_extenders) {
                for (const auto& extender : extenders) {
                    new_extenders.emplace_back(extender->clone(arc));
                }
            });

            return new_extender;
        }

        void extend(const Resource<ResourceType>& resource,
                    Resource<ResourceType>* extended_resource) const {
            this->extension_function_->extend(resource, *this, extended_resource);
        }

        void extend_back(const Resource<ResourceType>& resource,
                         Resource<ResourceType>* extended_resource) const {
            this->extension_function_->extend_back(resource, *this, extended_resource);
        }
};
}  // namespace rcspp
