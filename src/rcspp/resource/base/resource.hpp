// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <utility>

#include "rcspp/resource/base/resource_prototype.hpp"

namespace rcspp {

// Definition of ResourcePrototype for Resource
template <typename ResourceType>
class Resource : public ResourcePrototype<Resource<ResourceType>, ResourceType> {
        using Prototype = ResourcePrototype<Resource, ResourceType>;

    public:
        Resource() = default;

        Resource(const ResourceType& resource_base,
                 std::unique_ptr<DominanceFunction<ResourceType>> dominance_function,
                 std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
                 std::unique_ptr<CostFunction<ResourceType>> cost_function, std::size_t node_id = 0)
            : Prototype(resource_base, std::move(dominance_function),
                        std::move(feasibility_function), std::move(cost_function), node_id) {}

        Resource(std::unique_ptr<DominanceFunction<ResourceType>> dominance_function,
                 std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
                 std::unique_ptr<CostFunction<ResourceType>> cost_function, std::size_t node_id = 0)
            : Prototype(std::move(dominance_function), std::move(feasibility_function),
                        std::move(cost_function), node_id) {}

        Resource(const ResourceType& resource_base,
                 DominanceFunction<ResourceType>* dominance_function,
                 FeasibilityFunction<ResourceType>* feasibility_function,
                 CostFunction<ResourceType>* cost_function, std::size_t node_id = 0)
            : Prototype(resource_base, std::move(dominance_function),
                        std::move(feasibility_function), std::move(cost_function), node_id) {}

        Resource(DominanceFunction<ResourceType>* dominance_function,
                 FeasibilityFunction<ResourceType>* feasibility_function,
                 CostFunction<ResourceType>* cost_function, std::size_t node_id = 0)
            : Prototype(std::move(dominance_function), std::move(feasibility_function),
                        std::move(cost_function), node_id) {}

        Resource(Resource const& rhs_resource) : Prototype(rhs_resource) {}

        Resource(Resource&& rhs_resource) : Prototype(rhs_resource) {}
};
}  // namespace rcspp
