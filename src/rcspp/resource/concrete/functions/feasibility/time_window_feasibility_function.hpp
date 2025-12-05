// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <map>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

template <typename ResourceType,
          typename ValueType =
              std::decay_t<decltype(std::declval<Resource<ResourceType>>().get_value())>>
class TimeWindowFeasibilityFunction
    : public Clonable<TimeWindowFeasibilityFunction<ResourceType, ValueType>,
                      FeasibilityFunction<ResourceType>> {
    public:
        explicit TimeWindowFeasibilityFunction(
            const std::map<size_t, std::pair<ValueType, ValueType>>& time_window_by_node_id)
            : time_window_by_node_id_(time_window_by_node_id),
              max_time_window_(std::numeric_limits<ValueType>::max() / 2) {}  // prevent overflow

        [[nodiscard]] auto is_feasible(const Resource<ResourceType>& resource) -> bool override {
            return resource.get_value() <= max_time_window_;
        }

        [[nodiscard]] auto is_back_feasible(const Resource<ResourceType>& resource)
            -> bool override {
            return resource.get_value() >= min_time_window_;
        }

        [[nodiscard]] auto can_be_merged(const Resource<ResourceType>& resource,
                                         const Resource<ResourceType>& back_resource)
            -> bool override {
            return resource.get_value() <= back_resource.get_value();
        }

    private:
        const std::map<size_t, std::pair<ValueType, ValueType>>& time_window_by_node_id_;

        ValueType min_time_window_;
        ValueType max_time_window_;

        void preprocess(size_t node_id) override {
            auto& tw = time_window_by_node_id_.at(node_id);
            min_time_window_ = tw.first;
            max_time_window_ = tw.second;
        }
};
}  // namespace rcspp
