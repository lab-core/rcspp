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
class TimeWindowFeasibilityFunction
    : public Clonable<TimeWindowFeasibilityFunction<ResourceType, ValueType>,
                      FeasibilityFunction<ResourceType>> {
    public:
        explicit TimeWindowFeasibilityFunction(ValueType max_time_window)
            : max_time_window_by_node_id_(nullptr), max_time_window_(max_time_window) {}
        explicit TimeWindowFeasibilityFunction(
            const std::map<size_t, ValueType>* max_time_window_by_node_id,
            ValueType default_max_time_window = std::numeric_limits<ValueType>::max() /
                                                2)  // prevent overflow
            : max_time_window_by_node_id_(max_time_window_by_node_id),
              max_time_window_(default_max_time_window) {}

        auto is_feasible(const Resource<ResourceType>& resource) -> bool override {
            return resource.get_value() <= max_time_window_;
        }

    private:
        const std::map<size_t, ValueType>* const max_time_window_by_node_id_;

        ValueType max_time_window_;

        void preprocess(size_t node_id) override {
            if (max_time_window_by_node_id_ == nullptr) {
                return;
            }
            auto it = max_time_window_by_node_id_->find(node_id);
            // if found, update max_time_window_
            // else, keep previous max_time_window_ value
            if (it != max_time_window_by_node_id_->end()) {
                max_time_window_ = it->second;
            }
        }
};
}  // namespace rcspp
