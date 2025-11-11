// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/concrete/real_resource.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

class TimeWindowExtensionFunction
    : public Clonable<TimeWindowExtensionFunction, ExpansionFunction<RealResource>> {
    public:
        explicit TimeWindowExtensionFunction(
            const std::map<size_t, double>& min_time_window_by_arc_id);

        void extend(const Resource<RealResource>& resource, const Extender<RealResource>& extender,
                    Resource<RealResource>* extended_resource) override;

    private:
        const std::map<size_t, double>& min_time_window_by_arc_id_;
        double min_time_window_{0};

        void preprocess() override;
};
}  // namespace rcspp
