// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/composition/resource_value_composition.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"
#include "rcspp/utils/logger.hpp"

namespace rcspp {

template <typename ExtenderClass, typename ResourceType>
    requires std::derived_from<ResourceType, ResourceValue<ResourceType>>
class ExtenderPrototype {
    public:
        ExtenderPrototype() : value_(), extension_function_(nullptr), arc_id_(0) {}

        ExtenderPrototype(ResourceType resource_value,
                          std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                          const size_t arc_id)
            : value_(std::move(resource_value)),
              extension_function_(std::move(extension_function)),
              arc_id_(arc_id) {}

        template <typename... Args>
        ExtenderPrototype(const std::tuple<Args...>& resource_initializer,
                          std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                          const size_t arc_id)
            : value_(std::apply(
                  [](auto&&... args) {  // unpack arguments
                      return ResourceType(std::forward<decltype(args)>(args)...);
                  },
                  resource_initializer)),
              extension_function_(std::move(extension_function)),
              arc_id_(arc_id) {}

        ExtenderPrototype(std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                          const size_t arc_id)
            : value_(), extension_function_(std::move(extension_function)), arc_id_(arc_id) {}

        [[nodiscard]] auto clone() const -> std::unique_ptr<ExtenderClass> {
            return std::make_unique<ExtenderClass>(downcast());
        }

        [[nodiscard]] auto get_value() const -> const ResourceType& { return value_; }
        [[nodiscard]] auto get_value() -> ResourceType& { return value_; }

        [[nodiscard]] auto get_arc_id() const -> size_t { return arc_id_; }

        template <typename GraphResourceType>
        [[nodiscard]] auto clone(const Arc<GraphResourceType>& arc) const
            -> std::unique_ptr<ExtenderClass> {
            return std::make_unique<ExtenderClass>(value_,
                                                   extension_function_->create(arc),
                                                   arc.id);
        }

    protected:
        ResourceType value_;
        std::unique_ptr<ExtensionFunction<ResourceType>> extension_function_;

    private:
        const size_t arc_id_;

        [[nodiscard]] ExtenderClass& downcast() { return static_cast<ExtenderClass&>(*this); }

        [[nodiscard]] const ExtenderClass& downcast() const {
            return static_cast<ExtenderClass const&>(*this);
        }
};
}  // namespace rcspp
