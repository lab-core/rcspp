// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/concrete/real_resource.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

class TimeWindowFeasibilityFunction
    : public Clonable<TimeWindowFeasibilityFunction, FeasibilityFunction<RealResource>> {
    public:
        explicit TimeWindowFeasibilityFunction(
            const std::map<size_t, double>& max_time_window_by_node_id);

        auto is_feasible(const Resource<RealResource>& resource) -> bool override;

    private:
        const std::map<size_t, double>& max_time_window_by_node_id_;

        double max_time_window_;

        void preprocess() override;
};
}  // namespace rcspp