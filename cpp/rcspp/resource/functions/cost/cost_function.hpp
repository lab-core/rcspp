// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <utility>

#include "rcspp/resource/base/resource_type.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"

namespace rcspp {

template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Resource;

template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class CostFunction {
    public:
        virtual ~CostFunction() = default;

        [[nodiscard]] virtual auto get_cost(const ResourceType& resource) const -> double = 0;

        [[nodiscard]] virtual auto clone() const -> std::unique_ptr<CostFunction> = 0;

        auto create(const size_t node_id) -> std::unique_ptr<CostFunction> {
            auto new_cost_function = clone();
            new_cost_function->preprocess(node_id);
            return new_cost_function;
        }

        virtual void reset(const size_t node_id) { preprocess(node_id); }

    protected:
        virtual void preprocess(size_t node_id) {}
};

// Specialization for ResourceTypeComposition: functions receive the full Resource object.
template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class CostFunction<ResourceTypeComposition<ResourceTypes...>> {
    public:
        virtual ~CostFunction() = default;

        [[nodiscard]] virtual auto get_cost(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource) const
            -> double = 0;

        [[nodiscard]] virtual auto clone() const
            -> std::unique_ptr<CostFunction<ResourceTypeComposition<ResourceTypes...>>> = 0;

        auto create(const size_t node_id)
            -> std::unique_ptr<CostFunction<ResourceTypeComposition<ResourceTypes...>>> {
            auto new_cost_function = clone();
            new_cost_function->preprocess(node_id);
            return new_cost_function;
        }

        virtual void reset(const size_t node_id) { preprocess(node_id); }

    protected:
        virtual void preprocess(size_t node_id) {}
};

}  // namespace rcspp
