// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>
#include <type_traits>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

// Deduce the container/value type by calling get_value() on the concrete Resource
template <typename ResourceType,
          typename ValueType =
              std::decay_t<decltype(std::declval<Resource<ResourceType>>().get_value())>>
class IntersectFeasibilityFunction
    : public Clonable<IntersectFeasibilityFunction<ResourceType, ValueType>,
                      FeasibilityFunction<ResourceType>> {
    public:
        explicit IntersectFeasibilityFunction(
            const std::map<size_t, ValueType>& forbidden_by_node_id)
            : forbidden_by_node_id_(forbidden_by_node_id) {}

        [[nodiscard]] auto is_feasible(const Resource<ResourceType>& resource) -> bool override {
            return !resource.intersects(forbidden_.get_value());
        }

        [[nodiscard]] auto can_be_merged(const Resource<ResourceType>& resource,
                                         const Resource<ResourceType>& back_resource)
            -> bool override {
            // ensure that no node is in the intersection, as they would normally be visited on both
            // sides in this case
            return !resource.intersects(back_resource.get_value());
        }

    private:
        const std::map<size_t, ValueType>& forbidden_by_node_id_;
        ResourceType forbidden_;

        void preprocess(size_t node_id) override {
            forbidden_.set_value(forbidden_by_node_id_.at(node_id));
        }
};
}  // namespace rcspp
