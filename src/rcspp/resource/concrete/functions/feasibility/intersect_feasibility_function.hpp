// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>
#include <type_traits>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

template <typename ResourceType>
class IntersectFeasibilityFunction : public Clonable<IntersectFeasibilityFunction<ResourceType>,
                                                     FeasibilityFunction<ResourceType>> {
    public:
        // Deduce the container/value type by calling get_value() on the concrete Resource
        using ValueType =
            std::decay_t<decltype(std::declval<Resource<ResourceType>>().get_value())>;

        explicit IntersectFeasibilityFunction(
            const std::map<size_t, ValueType>& forbidden_by_node_id)
            : forbidden_by_node_id_(forbidden_by_node_id) {}

        auto is_feasible(const Resource<ResourceType>& resource) -> bool override {
            return !resource.intersects(forbidden_);
        }

    private:
        const std::map<size_t, ValueType>& forbidden_by_node_id_;
        ValueType forbidden_;

        void preprocess() override { forbidden_ = forbidden_by_node_id_.at(this->node_id_); }
};
}  // namespace rcspp
