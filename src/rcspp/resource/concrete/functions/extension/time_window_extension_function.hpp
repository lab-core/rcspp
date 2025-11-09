// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <map>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename ResourceType>
class TimeWindowExtensionFunction
    : public Clonable<TimeWindowExtensionFunction<ResourceType>, ExpansionFunction<ResourceType>> {
    public:
        explicit TimeWindowExtensionFunction(
            const std::map<size_t, double>& min_time_window_by_arc_id)
            : min_time_window_by_arc_id_(min_time_window_by_arc_id) {}

        void extend(const Resource<ResourceType>& resource, const Extender<ResourceType>& extender,
                    Resource<ResourceType>* extended_resource) override {
            double sum_value = resource.get_value() + extender.get_value();

            sum_value = std::max(min_time_window_, sum_value);

            extended_resource->set_value(sum_value);
        }

    private:
        const std::map<size_t, double>& min_time_window_by_arc_id_;
        double min_time_window_{0};

        void preprocess() override {
            min_time_window_ = min_time_window_by_arc_id_.at(this->arc_id_);
        }
};
}  // namespace rcspp
