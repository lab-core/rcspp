// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/cost/cost_function.hpp"

namespace rcspp {

template <size_t ResourceTypeIndex, typename... ResourceTypes>
class ComponentCostFunction
    : public Clonable<ComponentCostFunction<ResourceTypeIndex, ResourceTypes...>,
                      CostFunction<ResourceBaseComposition<ResourceTypes...>>> {
    public:
        explicit ComponentCostFunction(size_t resource_index) : resource_index_(resource_index) {}

        [[nodiscard]] double get_cost(
            const ResourceComposition<ResourceTypes...>& resource_composition) const override {
            const auto& resource =
                resource_composition.template get_resource_component<ResourceTypeIndex>(
                    resource_index_);

            auto cost = resource.get_cost();

            return cost;
        }

    private:
        size_t resource_index_;
};
}  // namespace rcspp
