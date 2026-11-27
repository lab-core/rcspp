// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <optional>
#include <typeinfo>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/cost/cost_function.hpp"

namespace rcspp {

/// @brief Cost function that extracts the cost from a single component of a composed resource.
///
/// Retrieves the sub-resource at position `resource_index_` in the type slot identified by
/// @p ResourceTypeIndex within the composed resource, then returns its cost.
///
/// @tparam ResourceTypeIndex Zero-based index of the target type slot in the composition.
/// @tparam ResourceTypes     The individual resource types forming the composition.
template <size_t ResourceTypeIndex, typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class ComponentCostFunction
    : public Clonable<ComponentCostFunction<ResourceTypeIndex, ResourceTypes...>,
                      CostFunction<ResourceTypeComposition<ResourceTypes...>>> {
    public:
        /// @brief Constructs the function to read cost from a specific sub-resource index.
        ///
        /// @param resource_index Position of the sub-resource within the type slot's vector.
        explicit ComponentCostFunction(size_t resource_index) : resource_index_(resource_index) {}

        /// @brief Returns the cost of the targeted sub-resource.
        ///
        /// @param resource_composition The composed resource to query.
        /// @return Cost of the sub-resource at `resource_index_` in slot @p ResourceTypeIndex.
        [[nodiscard]] double get_cost(const Resource<ResourceTypeComposition<ResourceTypes...>>&
                                          resource_composition) const override {
            const auto& resource =
                resource_composition.template get_component<ResourceTypeIndex>(resource_index_);

            auto cost = resource.get_cost();

            return cost;
        }

        /// @brief Whether this cost is additive: that of the one component it reads.
        ///
        /// @param resource_composition Any node's resource.
        /// @return See @c Resource::is_cost_additive.
        [[nodiscard]] auto is_additive(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource_composition) const
            -> bool override {
            return resource_composition.template get_component<ResourceTypeIndex>(resource_index_)
                .is_cost_additive();
        }

        /// @brief The cost order of the one component this function reads: its
        ///        @c Resource::component_cost_order_tolerance (or @c
        ///        back_component_cost_order_tolerance).
        ///
        /// @param resource_composition Any node's resource.
        /// @param backward             Whether the components backward-dominate.
        /// @return The tolerance, or @c std::nullopt; none for a class derived from this one,
        ///         which may compute its cost otherwise.
        [[nodiscard]] auto component_wise_cost_order_tolerance(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource_composition,
            bool backward) const -> std::optional<double> override {
            if (typeid(*this) != typeid(ComponentCostFunction)) {
                return std::nullopt;
            }
            const auto& component =
                resource_composition.template get_component<ResourceTypeIndex>(resource_index_);
            return backward ? component.back_component_cost_order_tolerance()
                            : component.component_cost_order_tolerance();
        }

    private:
        size_t resource_index_;
};
}  // namespace rcspp
