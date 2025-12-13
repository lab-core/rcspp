// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/composition/resource_base_composition.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename ExtenderClass, typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class ExtenderPrototype : public ResourceType {
    public:
        ExtenderPrototype() : extension_function_(nullptr), arc_id_(0) {}

        ExtenderPrototype(const ResourceType& resource_base,
                          std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                          const size_t arc_id)
            : ResourceType(resource_base),
              extension_function_(std::move(extension_function)),
              arc_id_(arc_id) {}

        template <typename... Args>
        ExtenderPrototype(const std::tuple<Args...>& resource_initializer,
                          std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                          const size_t arc_id)
            : ResourceType(std::apply(
                  [](auto&&... args) {  // unpack arguments
                      return ResourceType(std::forward<decltype(args)>(args)...);
                  },
                  resource_initializer)),
              extension_function_(std::move(extension_function)),
              arc_id_(arc_id) {}

        ExtenderPrototype(std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                          const size_t arc_id)
            : extension_function_(std::move(extension_function)), arc_id_(arc_id) {}

        [[nodiscard]] auto clone() const -> std::unique_ptr<ExtenderClass> {
            return std::make_unique<ExtenderClass>(downcast());
        }

        // Resource extension
        void extend(const Resource<ResourceType>& resource,
                    Resource<ResourceType>* extended_resource) const {
            extension_function_->extend(resource, downcast(), extended_resource);
        }

        void extend_back(const Resource<ResourceType>& resource,
                         Resource<ResourceType>* extended_resource) const {
            extension_function_->extend_back(resource, downcast(), extended_resource);
        }

        [[nodiscard]] auto get_arc_id() const -> size_t { return arc_id_; }

        template <typename GraphResourceType>
        [[nodiscard]] auto clone(const Arc<GraphResourceType>& arc) const
            -> std::unique_ptr<ExtenderClass> {
            return std::make_unique<ExtenderClass>(downcast(),
                                                   extension_function_->create(arc),
                                                   arc.id);
        }

    private:
        std::unique_ptr<ExtensionFunction<ResourceType>> extension_function_;

        const size_t arc_id_;

        [[nodiscard]] ExtenderClass& downcast() { return static_cast<ExtenderClass&>(*this); }

        [[nodiscard]] const ExtenderClass& downcast() const {
            return static_cast<ExtenderClass const&>(*this);
        }
};
}  // namespace rcspp
