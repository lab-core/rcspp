// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <utility>

#include "rcspp/resource/composition/resource_value_composition.hpp"

namespace rcspp {

template <typename ResourceType>
class Resource;

template <typename ResourceType>
class DominanceFunction {
    public:
        virtual ~DominanceFunction() = default;

        [[nodiscard]] virtual auto check_dominance(const ResourceType& lhs_resource,
                                                   const ResourceType& rhs_resource) -> bool = 0;

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

// Specialization for ResourceBaseComposition: functions receive the full Resource object
// since the composition tag carries no values of its own.
template <typename... ResourceTypes>
class DominanceFunction<ResourceValueComposition<ResourceTypes...>> {
    public:
        virtual ~DominanceFunction() = default;

        [[nodiscard]] virtual auto check_dominance(
            const Resource<ResourceValueComposition<ResourceTypes...>>& lhs_resource,
            const Resource<ResourceValueComposition<ResourceTypes...>>& rhs_resource) -> bool = 0;

        [[nodiscard]] virtual auto clone() const
            -> std::unique_ptr<DominanceFunction<ResourceValueComposition<ResourceTypes...>>> = 0;

        auto create(const size_t node_id)
            -> std::unique_ptr<DominanceFunction<ResourceValueComposition<ResourceTypes...>>> {
            auto new_dominance_function = clone();
            new_dominance_function->preprocess(node_id);
            return new_dominance_function;
        }

        virtual void reset(const size_t node_id) { preprocess(node_id); }

    protected:
        virtual void preprocess(size_t node_id) {}
};

}  // namespace rcspp
