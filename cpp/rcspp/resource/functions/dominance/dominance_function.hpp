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
class DominanceFunction {
    public:
        virtual ~DominanceFunction() = default;

        [[nodiscard]] virtual auto check_dominance(const ResourceType& lhs_resource,
                                                   const ResourceType& rhs_resource) -> bool = 0;

        // clang-format off
        // Use to check (partial) dominance quickly. Useful for more complex data structure
        virtual auto fast_check_dominance(const ResourceType& lhs_resource,
                                          const ResourceType& rhs_resource, double delta)
            -> bool = 0;
        // clang-format on

        [[nodiscard]] virtual auto clone() const -> std::unique_ptr<DominanceFunction> = 0;

        auto create(const size_t node_id) -> std::unique_ptr<DominanceFunction> {
            auto new_dominance_function = clone();
            new_dominance_function->preprocess(node_id);
            return new_dominance_function;
        }

        virtual void reset(const size_t node_id) { preprocess(node_id); }

    protected:
        virtual void preprocess(size_t node_id) {}
};

// Specialization for ResourceTypeComposition: functions receive the full Resource object
// since the composition tag carries no values of its own.
template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class DominanceFunction<ResourceTypeComposition<ResourceTypes...>> {
    public:
        virtual ~DominanceFunction() = default;

        [[nodiscard]] virtual auto check_dominance(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& lhs_resource,
            const Resource<ResourceTypeComposition<ResourceTypes...>>& rhs_resource) -> bool = 0;

        [[nodiscard]] virtual auto clone() const
            -> std::unique_ptr<DominanceFunction<ResourceTypeComposition<ResourceTypes...>>> = 0;

        auto create(const size_t node_id)
            -> std::unique_ptr<DominanceFunction<ResourceTypeComposition<ResourceTypes...>>> {
            auto new_dominance_function = clone();
            new_dominance_function->preprocess(node_id);
            return new_dominance_function;
        }

        virtual void reset(const size_t node_id) { preprocess(node_id); }

    protected:
        virtual void preprocess(size_t node_id) {}
};

}  // namespace rcspp
