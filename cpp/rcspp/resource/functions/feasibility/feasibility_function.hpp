// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <stdexcept>
#include <utility>

#include "rcspp/resource/base/resource_type.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"

namespace rcspp {

template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Resource;

template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class FeasibilityFunction {
    public:
        virtual ~FeasibilityFunction() = default;

        [[nodiscard]] virtual auto is_feasible(const ResourceType& resource) -> bool = 0;

        // GCOVR_EXCL_START — back-direction not used in tests
        [[nodiscard]] virtual auto is_back_feasible(const ResourceType& resource) -> bool {
            return is_feasible(resource);
        }
        // GCOVR_EXCL_STOP

        [[nodiscard]] virtual auto can_be_merged(const ResourceType& resource,
                                                 const ResourceType& back_resource) -> bool {
            throw std::runtime_error(
                "FeasibilityFunction::can_be_merged not implemented");  // GCOVR_EXCL_LINE
        };

        // GCOVR_EXCL_START — is_reachable default never called in tests
        virtual auto is_reachable(const Resource<ResourceType>& resource,
                                  size_t destination_node_id) -> bool {
            return true;
        }
        // GCOVR_EXCL_STOP

        [[nodiscard]] virtual auto clone() const -> std::unique_ptr<FeasibilityFunction> = 0;

        // GCOVR_EXCL_START — clone+preprocess default not called directly in tests
        virtual auto create(const size_t node_id) -> std::unique_ptr<FeasibilityFunction> {
            auto new_feasibility_function = clone();
            new_feasibility_function->preprocess(node_id);
            return new_feasibility_function;
        }
        // GCOVR_EXCL_STOP

        virtual void reset(const size_t node_id) { preprocess(node_id); }

    protected:
        virtual void preprocess(size_t node_id) {}
};

// Specialization for ResourceTypeComposition: functions receive the full Resource object.
template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>> {
    public:
        virtual ~FeasibilityFunction() = default;

        [[nodiscard]] virtual auto is_feasible(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource) -> bool = 0;

        // GCOVR_EXCL_START — back-direction not used in tests
        [[nodiscard]] virtual auto is_back_feasible(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource) -> bool {
            return is_feasible(resource);
        }
        // GCOVR_EXCL_STOP

        [[nodiscard]] virtual auto can_be_merged(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource,
            const Resource<ResourceTypeComposition<ResourceTypes...>>& back_resource) -> bool {
            throw std::runtime_error(
                "FeasibilityFunction::can_be_merged not implemented");  // GCOVR_EXCL_LINE
        };

        virtual auto is_reachable(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& /*resource*/,
            size_t /*destination_node_id*/) -> bool {
            return true;
        }

        [[nodiscard]] virtual auto clone() const
            -> std::unique_ptr<FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>>> = 0;

        // GCOVR_EXCL_START — clone+preprocess default not called directly in tests
        virtual auto create(const size_t node_id)
            -> std::unique_ptr<FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>>> {
            auto new_feasibility_function = clone();
            new_feasibility_function->preprocess(node_id);
            return new_feasibility_function;
        }
        // GCOVR_EXCL_STOP

        virtual void reset(const size_t node_id) { preprocess(node_id); }

    protected:
        virtual void preprocess(size_t node_id) {}
};

}  // namespace rcspp
