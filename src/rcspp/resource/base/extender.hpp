// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename ResourceType>
// requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class Extender : public ResourceType {
    public:
        Extender() : extension_function_(nullptr), arc_id_(0) {}

        Extender(const ResourceType& resource_base,
                 std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                 const size_t arc_id)
            : ResourceType(resource_base),
              extension_function_(std::move(extension_function)),
              arc_id_(arc_id) {}

        Extender(std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                 const size_t arc_id)
            : extension_function_(std::move(extension_function)), arc_id_(arc_id) {}

        // Resource extension
        void extend(const Resource<ResourceType>& resource,
                    Resource<ResourceType>* extended_resource) const {
            extension_function_->extend(resource, *this, extended_resource);
        }

        [[nodiscard]] auto get_arc_id() const -> size_t { return arc_id_; }

        [[nodiscard]] auto create(const ResourceType& resource_base, const size_t arc_id) const
            -> std::unique_ptr<Extender<ResourceType>> {
            auto new_extender = std::make_unique<Extender>(resource_base,
                                                           extension_function_->create(arc_id),
                                                           arc_id);

            return new_extender;
        }

        [[nodiscard]] auto create(const size_t arc_id) const
            -> std::unique_ptr<Extender<ResourceType>> {
            auto new_extender =
                std::make_unique<Extender>(extension_function_->create(arc_id), arc_id);

            return new_extender;
        }

    private:
        std::unique_ptr<ExtensionFunction<ResourceType>> extension_function_;

        const size_t arc_id_;
};

// Specialization for ResourceComposition
// TODO(patrick): Find a way to avoid code duplication
template <typename... ResourceTypes>
// requires (std::derived_from<ResourceTypes, ResourceBase<ResourceTypes>> && ...)
class Extender<ResourceComposition<ResourceTypes...>>
    : public ResourceComposition<ResourceTypes...> {
        friend class ResourceCompositionFactory<ResourceTypes...>;

    public:
        Extender() : extension_function_(nullptr), arc_id_(0) {}

        Extender(const ResourceComposition<ResourceTypes...>& resource_base,
                 std::unique_ptr<ExtensionFunction<ResourceComposition<ResourceTypes...>>>
                     extension_function,
                 const size_t arc_id)
            : ResourceComposition<ResourceTypes...>(resource_base),
              extension_function_(std::move(extension_function)),
              arc_id_(arc_id) {}

        Extender(std::unique_ptr<ExtensionFunction<ResourceComposition<ResourceTypes...>>>
                     extension_function,
                 const size_t arc_id)
            : extension_function_(std::move(extension_function)), arc_id_(arc_id) {}

        // Resource extension
        void extend(const Resource<ResourceComposition<ResourceTypes...>>& resource,
                    Resource<ResourceComposition<ResourceTypes...>>* extended_resource) const {
            extension_function_->extend(resource, *this, extended_resource);
        }

        [[nodiscard]] auto get_arc_id() const -> size_t { return arc_id_; }

        [[nodiscard]] auto create(const ResourceComposition<ResourceTypes...>& resource_base,
                                  const size_t arc_id) const
            -> std::unique_ptr<Extender<ResourceComposition<ResourceTypes...>>> {
            auto new_extender = std::make_unique<Extender>(resource_base,
                                                           extension_function_->create(arc_id),
                                                           arc_id);

            return new_extender;
        }

        [[nodiscard]] auto create(const size_t arc_id) const
            -> std::unique_ptr<Extender<ResourceComposition<ResourceTypes...>>> {
            auto new_extender =
                std::make_unique<Extender>(extension_function_->create(arc_id), arc_id);

            return new_extender;
        }

        // New method
        [[nodiscard]] auto get_extender_components()
            -> std::tuple<std::vector<std::unique_ptr<Extender<ResourceTypes>>>...>& {
            return extender_components_;
        }

        [[nodiscard]] auto get_extender_components() const
            -> const std::tuple<std::vector<std::unique_ptr<Extender<ResourceTypes>>>...>& {
            return extender_components_;
        }

    private:
        // New attribute
        std::tuple<std::vector<std::unique_ptr<Extender<ResourceTypes>>>...> extender_components_;

        std::unique_ptr<ExtensionFunction<ResourceComposition<ResourceTypes...>>>
            extension_function_;

        const size_t arc_id_;
};
}  // namespace rcspp
