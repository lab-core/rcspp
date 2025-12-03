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

        template <typename GraphResourceType>
        [[nodiscard]] auto clone(const Arc<GraphResourceType>& arc) const
            -> std::unique_ptr<Extender<ResourceType>> {
            return std::make_unique<Extender<ResourceType>>(*this,
                                                            extension_function_->create(arc),
                                                            arc.id);
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

        [[nodiscard]] auto clone(const Arc<ResourceComposition<ResourceTypes...>>& arc) const
            -> std::unique_ptr<Extender<ResourceComposition<ResourceTypes...>>> {
            auto new_extender = std::make_unique<Extender<ResourceComposition<ResourceTypes...>>>(
                *this,
                extension_function_->create(arc),
                arc.id);

            auto make_extender_function = [&](const auto& extenders, auto& new_extenders) {
                for (const auto& extender : extenders) {
                    new_extenders.emplace_back(extender->clone(arc));
                }
            };

            std::apply(
                [&](const auto&... ext_comp) {
                    std::apply(
                        [&](auto&... new_ext_comp) {
                            (make_extender_function(ext_comp, new_ext_comp), ...);
                        },
                        new_extender->extender_components_);
                },
                extender_components_);

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

        template <size_t ResourceTypeIndex>
        [[nodiscard]] auto get_extender_components() -> auto& {
            return std::get<ResourceTypeIndex>(extender_components_);
        }

        template <size_t ResourceTypeIndex>
        [[nodiscard]] auto get_extender_components() const -> const auto& {
            return std::get<ResourceTypeIndex>(extender_components_);
        }

        template <size_t ResourceTypeIndex>
        [[nodiscard]] auto get_extender_component(size_t resource_index) const -> const auto& {
            return *(std::get<ResourceTypeIndex>(extender_components_)[resource_index]);
        }

        template <typename ResourceType>
        [[nodiscard]] auto get_extender_components() -> auto& {
            constexpr size_t ResourceTypeIndex =
                ResourceTypeIndex_v<ResourceType, ResourceTypes...>;
            return get_extender_components<ResourceTypeIndex>();
        }

        template <typename ResourceType>
        [[nodiscard]] auto get_extender_components() const -> const auto& {
            constexpr size_t ResourceTypeIndex =
                ResourceTypeIndex_v<ResourceType, ResourceTypes...>;
            return get_extender_components<ResourceTypeIndex>();
        }

        template <typename ResourceType>
        [[nodiscard]] auto get_extender_component(size_t resource_index) const -> const auto& {
            constexpr size_t ResourceTypeIndex =
                ResourceTypeIndex_v<ResourceType, ResourceTypes...>;
            return get_extender_component<ResourceTypeIndex>(resource_index);
        }

    private:
        // New attribute
        std::tuple<std::vector<std::unique_ptr<Extender<ResourceTypes>>>...> extender_components_;

        std::unique_ptr<ExtensionFunction<ResourceComposition<ResourceTypes...>>>
            extension_function_;

        const size_t arc_id_;
};
}  // namespace rcspp
