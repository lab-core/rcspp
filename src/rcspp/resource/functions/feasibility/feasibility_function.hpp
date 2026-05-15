// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <stdexcept>
#include <utility>

#include "rcspp/resource/composition/resource_type_composition.hpp"

namespace rcspp {

template <typename ResourceType>
class Resource;

template <typename ResourceType>
class FeasibilityFunction {
    public:
        virtual ~FeasibilityFunction() = default;

        [[nodiscard]] virtual auto is_feasible(const ResourceType& resource) -> bool = 0;

        [[nodiscard]] virtual auto is_back_feasible(const ResourceType& resource) -> bool {
            return is_feasible(resource);
        }

        [[nodiscard]] virtual auto can_be_merged(const ResourceType& resource,
                                                 const ResourceType& back_resource) -> bool {
            throw std::runtime_error("FeasibilityFunction::can_be_merged not implemented");
        };

        virtual auto is_reachable(const Resource<ResourceType>& resource,
                                  size_t destination_node_id) -> bool {
            return true;
        }

        [[nodiscard]] virtual auto clone() const -> std::unique_ptr<FeasibilityFunction> = 0;

        virtual auto create(const size_t node_id) -> std::unique_ptr<FeasibilityFunction> {
            auto new_feasibility_function = clone();
            new_feasibility_function->preprocess(node_id);
            return new_feasibility_function;
        }

        virtual void reset(const size_t node_id) { preprocess(node_id); }

    protected:
        virtual void preprocess(size_t node_id) {}
};

// Specialization for ResourceTypeComposition: functions receive the full Resource object.
template <typename... ResourceTypes>
class FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>> {
    public:
        virtual ~FeasibilityFunction() = default;

        [[nodiscard]] virtual auto is_feasible(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource) -> bool = 0;

        [[nodiscard]] virtual auto is_back_feasible(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource) -> bool {
            return is_feasible(resource);
        }

        [[nodiscard]] virtual auto can_be_merged(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource,
            const Resource<ResourceTypeComposition<ResourceTypes...>>& back_resource) -> bool {
            throw std::runtime_error("FeasibilityFunction::can_be_merged not implemented");
        };

        virtual auto is_reachable(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& /*resource*/,
            size_t /*destination_node_id*/) -> bool {
            return true;
        }

        [[nodiscard]] virtual auto clone() const
            -> std::unique_ptr<FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>>> = 0;

        virtual auto create(const size_t node_id)
            -> std::unique_ptr<FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>>> {
            auto new_feasibility_function = clone();
            new_feasibility_function->preprocess(node_id);
            return new_feasibility_function;
        }

        virtual void reset(const size_t node_id) { preprocess(node_id); }

    protected:
        virtual void preprocess(size_t node_id) {}
};

}  // namespace rcspp
