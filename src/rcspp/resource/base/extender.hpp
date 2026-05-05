// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <tuple>
#include <utility>

#include "rcspp/resource/base/extender_prototype.hpp"

namespace rcspp {

// Definition of ExtenderPrototype for Extender
template <typename ResourceType>
class Extender : public ExtenderPrototype<Extender<ResourceType>, ResourceType> {
        using Prototype = ExtenderPrototype<Extender, ResourceType>;

    public:
        Extender(const ResourceType& resource_value,
                 std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                 const size_t arc_id)
            : Prototype(resource_value, std::move(extension_function), arc_id) {}

        template <typename... Args>
        Extender(const std::tuple<Args...>& resource_initializer,
                 std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                 const size_t arc_id)
            : Prototype(resource_initializer, std::move(extension_function), arc_id) {}

        Extender(std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                 const size_t arc_id)
            : Prototype(std::move(extension_function), arc_id) {}

        // Resource extension
        void extend(const Resource<ResourceType>& resource,
                    Resource<ResourceType>* extended_resource) const {
            this->extension_function_->extend(resource.get_value(),
                                              this->value_,
                                              &extended_resource->get_value());
        }

        void extend_back(const Resource<ResourceType>& resource,
                         Resource<ResourceType>* extended_resource) const {
            this->extension_function_->extend_back(resource.get_value(),
                                                   this->value_,
                                                   &extended_resource->get_value());
        }
};
}  // namespace rcspp
