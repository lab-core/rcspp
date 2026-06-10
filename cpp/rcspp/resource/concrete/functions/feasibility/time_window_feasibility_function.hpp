// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <map>
#include <memory>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

template <typename ResourceType,
          typename ValueType = std::decay_t<decltype(std::declval<ResourceType>().get_value())>>
class TimeWindowFeasibilityFunction
    : public Clonable<TimeWindowFeasibilityFunction<ResourceType, ValueType>,
                      FeasibilityFunction<ResourceType>> {
    public:
        explicit TimeWindowFeasibilityFunction(
            std::map<size_t, std::pair<ValueType, ValueType>> time_window_by_node_id,
            ValueType default_max_time_window = std::numeric_limits<ValueType>::max() /
                                                2)  // prevent overflow
            : time_window_by_node_id_(
                  std::make_shared<const std::map<size_t, std::pair<ValueType, ValueType>>>(
                      std::move(time_window_by_node_id))),
              default_max_time_window_(default_max_time_window),
              max_time_window_(default_max_time_window) {}

        [[nodiscard]] auto is_feasible(const ResourceType& resource) -> bool override {
            return resource.get_value() <= max_time_window_;
        }

        [[nodiscard]] auto is_back_feasible(const ResourceType& resource) -> bool override {
            return resource.get_value() >= min_time_window_;
        }

        [[nodiscard]] auto can_be_merged(const ResourceType& resource,
                                         const ResourceType& back_resource) -> bool override {
            return resource.get_value() <= back_resource.get_value();
        }

    private:
        std::shared_ptr<const std::map<size_t, std::pair<ValueType, ValueType>>>
            time_window_by_node_id_;
        ValueType default_min_time_window_{0};
        ValueType default_max_time_window_{};
        ValueType min_time_window_{0};
        ValueType max_time_window_{};

        void preprocess(size_t node_id) override {
            auto it = time_window_by_node_id_->find(node_id);
            if (it != time_window_by_node_id_->end()) {
                min_time_window_ = it->second.first;
                max_time_window_ = it->second.second;
            } else {
                min_time_window_ = default_min_time_window_;
                max_time_window_ = default_max_time_window_;
            }
        }
};
}  // namespace rcspp
