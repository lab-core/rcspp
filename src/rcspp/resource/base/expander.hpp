// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <memory>
#include <tuple>
#include <utility>

#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/expansion/expansion_function.hpp"

namespace rcspp {

template <typename ResourceType>
// requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class Expander : public ResourceType {
    public:
        Expander() : expansion_function_(nullptr), arc_id_(0) {}

        Expander(const ResourceType& resource_base,
                 std::unique_ptr<ExpansionFunction<ResourceType>> expansion_function,
                 const size_t arc_id)
            : ResourceType(resource_base),
              expansion_function_(std::move(expansion_function)),
              arc_id_(arc_id) {}

        Expander(std::unique_ptr<ExpansionFunction<ResourceType>> expansion_function,
                 const size_t arc_id)
            : expansion_function_(std::move(expansion_function)), arc_id_(arc_id) {}

        // Resource expansion
        void expand(const Resource<ResourceType>& resource,
                    Resource<ResourceType>& expanded_resource) const {
            expansion_function_->expand(resource, *this, expanded_resource);
        }

        [[nodiscard]] auto get_arc_id() const -> size_t { return arc_id_; }

        [[nodiscard]] auto create(const ResourceType& resource_base, const size_t arc_id) const
            -> std::unique_ptr<Expander<ResourceType>> {
            auto new_expander = std::make_unique<Expander>(resource_base,
                                                           expansion_function_->create(arc_id),
                                                           arc_id);

            return new_expander;
        }

        [[nodiscard]] auto create(const size_t arc_id) const
            -> std::unique_ptr<Expander<ResourceType>> {
            auto new_expander =
                std::make_unique<Expander>(expansion_function_->create(arc_id), arc_id);

            return new_expander;
        }

    private:
        std::unique_ptr<ExpansionFunction<ResourceType>> expansion_function_;

        const size_t arc_id_;
};

// Specialization for ResourceComposition
// TODO(patrick): Find a way to avoid code duplication
template <typename... ResourceTypes>
// requires (std::derived_from<ResourceTypes, ResourceBase<ResourceTypes>> && ...)
class Expander<ResourceComposition<ResourceTypes...>>
    : public ResourceComposition<ResourceTypes...> {
        friend class ResourceCompositionFactory<ResourceTypes...>;

    public:
        Expander() : expansion_function_(nullptr), arc_id_(0) {}

        Expander(const ResourceComposition<ResourceTypes...>& resource_base,
                 std::unique_ptr<ExpansionFunction<ResourceComposition<ResourceTypes...>>>
                     expansion_function,
                 const size_t arc_id)
            : ResourceComposition<ResourceTypes...>(resource_base),
              expansion_function_(std::move(expansion_function)),
              arc_id_(arc_id) {}

        Expander(std::unique_ptr<ExpansionFunction<ResourceComposition<ResourceTypes...>>>
                     expansion_function,
                 const size_t arc_id)
            : expansion_function_(std::move(expansion_function)), arc_id_(arc_id) {}

        // Resource expansion
        void expand(const Resource<ResourceComposition<ResourceTypes...>>& resource,
                    Resource<ResourceComposition<ResourceTypes...>>& expanded_resource) const {
            expansion_function_->expand(resource, *this, expanded_resource);
        }

        [[nodiscard]] auto get_arc_id() const -> size_t { return arc_id_; }

        [[nodiscard]] auto create(const ResourceComposition<ResourceTypes...>& resource_base,
                                  const size_t arc_id) const
            -> std::unique_ptr<Expander<ResourceComposition<ResourceTypes...>>> {
            auto new_expander = std::make_unique<Expander>(resource_base,
                                                           expansion_function_->create(arc_id),
                                                           arc_id);

            return new_expander;
        }

        [[nodiscard]] auto create(const size_t arc_id) const
            -> std::unique_ptr<Expander<ResourceComposition<ResourceTypes...>>> {
            auto new_expander =
                std::make_unique<Expander>(expansion_function_->create(arc_id), arc_id);

            return new_expander;
        }

        // New method
        [[nodiscard]] auto get_expander_components()
            -> std::tuple<std::vector<std::unique_ptr<Expander<ResourceTypes>>>...>& {
            return expander_components_;
        }

        [[nodiscard]] auto get_expander_components() const
            -> const std::tuple<std::vector<std::unique_ptr<Expander<ResourceTypes>>>...>& {
            return expander_components_;
        }

    private:
        // New attribute
        std::tuple<std::vector<std::unique_ptr<Expander<ResourceTypes>>>...> expander_components_;

        std::unique_ptr<ExpansionFunction<ResourceComposition<ResourceTypes...>>>
            expansion_function_;

        const size_t arc_id_;
};
}  // namespace rcspp