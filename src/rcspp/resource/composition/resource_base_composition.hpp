// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <concepts>  // NOLINT(build/include_order)
#include <iterator>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/resource/base/resource_base.hpp"
#include "rcspp/resource/composition/composition.hpp"

namespace rcspp {

template <typename... ResourceTypes>
    requires(std::derived_from<ResourceTypes, ResourceBase<ResourceTypes>> && ...)
class ResourceBaseComposition : public ResourceBase<ResourceBaseComposition<ResourceTypes...>>,
                                private Composition<ResourceTypes...> {  // just storing the types
        template <typename... Types>
        friend class ResourceBaseCompositionFactory;

    public:
        ResourceBaseComposition() = default;

        explicit ResourceBaseComposition(
            std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...> resource_base_components)
            : Composition<ResourceTypes...>(std::move(resource_base_components)) {}

        // Copy constructor
        ResourceBaseComposition(const ResourceBaseComposition& rhs_resource_composition)
            : Composition<ResourceTypes...>(rhs_resource_composition) {}

        ResourceBaseComposition(ResourceBaseComposition&& rhs_resource_composition)
            : Composition<ResourceTypes...>(rhs_resource_composition) {}

        ~ResourceBaseComposition() override = default;

        auto operator=(ResourceBaseComposition rhs_resource_composition)
            -> ResourceBaseComposition& {
            this->swap(*this, rhs_resource_composition);
            return *this;
        }

        // Add (move) the resource in argument to the right vector of resources (i.e.,
        // ResourceTypeIndex).
        template <size_t ResourceTypeIndex, typename ResourceType>
        auto add_component(std::unique_ptr<ResourceType> resource) -> ResourceType& {
            return this->template add_component<ResourceTypeIndex>(std::move(resource));
        }

        // Construct a resource from a list of arguments associated with a constructor (of
        // ResourceType).
        template <size_t ResourceTypeIndex, typename ResourceType, typename TypeTuple>
        auto add_component(const TypeTuple& resource_initializer) -> ResourceType& {
            auto res_init_index = std::make_index_sequence<std::tuple_size_v<
                typename std::remove_reference_t<decltype(resource_initializer)>>>{};

            auto resource =
                make_single_resource<ResourceType>(resource_initializer, res_init_index);
            return this->template add_component<ResourceTypeIndex>(std::move(resource));
        }

        void reset() override {
            this->for_each_component([](auto& component) { component.reset(); });
        }
};
}  // namespace rcspp
