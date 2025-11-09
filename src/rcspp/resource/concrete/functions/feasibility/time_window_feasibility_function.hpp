// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <map>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

template <typename ResourceType>
class TimeWindowFeasibilityFunction : public Clonable<TimeWindowFeasibilityFunction<ResourceType>,
                                                      FeasibilityFunction<ResourceType>> {
    public:
        explicit TimeWindowFeasibilityFunction(
            const std::map<size_t, double>& max_time_window_by_node_id)
            : max_time_window_by_node_id_(max_time_window_by_node_id),
              max_time_window_(std::numeric_limits<double>::infinity()) {}

        auto is_feasible(const Resource<ResourceType>& resource) -> bool override {
            bool feasible = true;

            if (resource.get_value() > max_time_window_) {
                feasible = false;
            }

            return feasible;
        }

    private:
        const std::map<size_t, double>& max_time_window_by_node_id_;

        double max_time_window_;

        void preprocess() override {
            max_time_window_ = max_time_window_by_node_id_.at(this->node_id_);
        }
};
}  // namespace rcspp
