// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/concrete/real_resource.hpp"
#include "rcspp/resource/functions/expansion/expansion_function.hpp"

namespace rcspp {

class TimeWindowExpansionFunction
    : public Clonable<TimeWindowExpansionFunction, ExpansionFunction<RealResource>> {
    public:
        explicit TimeWindowExpansionFunction(
            const std::map<size_t, double>& min_time_window_by_arc_id);

        void expand(const Resource<RealResource>& resource, const Expander<RealResource>& expander,
                    Resource<RealResource>* expanded_resource) override;

    private:
        const std::map<size_t, double>& min_time_window_by_arc_id_;
        double min_time_window_{0};

        void preprocess() override;
};
}  // namespace rcspp
