// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <map>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename ResourceType,
          typename ValueType =
              std::decay_t<decltype(std::declval<Resource<ResourceType>>().get_value())>>
class TimeWindowExtensionFunction
    : public Clonable<TimeWindowExtensionFunction<ResourceType>, ExtensionFunction<ResourceType>> {
    public:
        explicit TimeWindowExtensionFunction(
            const std::map<size_t, ValueType>& min_time_window_by_dest_id)
            : min_time_window_by_dest_id_(min_time_window_by_dest_id) {}

        void extend(const Resource<ResourceType>& resource, const Extender<ResourceType>& extender,
                    Resource<ResourceType>* extended_resource) override {
            auto sum_value = resource.get_value() + extender.get_value();
            sum_value = std::max(min_time_window_, sum_value);
            extended_resource->set_value(sum_value);
        }

    private:
        const std::map<size_t, ValueType>& min_time_window_by_dest_id_;
        ValueType min_time_window_{0};

        template <typename GraphResourceType>
        void preprocess(const Arc<GraphResourceType>& arc) {
            min_time_window_ = min_time_window_by_dest_id_.at(arc.destination->id);
        }
};
}  // namespace rcspp
