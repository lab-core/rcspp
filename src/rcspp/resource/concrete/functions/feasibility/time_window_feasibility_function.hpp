// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <map>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

template <typename ResourceType,
          typename ValueType =
              std::decay_t<decltype(std::declval<Resource<ResourceType>>().get_value())>>
class TimeWindowFeasibilityFunction : public Clonable<TimeWindowFeasibilityFunction<ResourceType>,
                                                      FeasibilityFunction<ResourceType>> {
    public:
        explicit TimeWindowFeasibilityFunction(
            const std::map<size_t, ValueType>& max_time_window_by_node_id)
            : max_time_window_by_node_id_(max_time_window_by_node_id),
              max_time_window_(std::numeric_limits<ValueType>::max() / 2) {}

        auto is_feasible(const Resource<ResourceType>& resource) -> bool override {
            return resource.get_value() <= max_time_window_;
        }

    private:
        const std::map<size_t, ValueType>& max_time_window_by_node_id_;

        ValueType max_time_window_;

        void preprocess() override {
            max_time_window_ = max_time_window_by_node_id_.at(this->node_id_);
        }
};
}  // namespace rcspp
