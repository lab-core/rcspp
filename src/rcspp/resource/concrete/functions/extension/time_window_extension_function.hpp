// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <map>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename ResourceType,
          typename ValueType =
              std::decay_t<decltype(std::declval<Resource<ResourceType>>().get_value())>>
class TimeWindowExtensionFunction
    : public Clonable<TimeWindowExtensionFunction<ResourceType, ValueType>,
                      ExtensionFunction<ResourceType>> {
    public:
        explicit TimeWindowExtensionFunction(
            const std::map<size_t, std::pair<ValueType, ValueType>>& time_window_by_node_id)
            : time_window_by_node_id_(time_window_by_node_id) {}

        void extend(const Resource<ResourceType>& resource, const Extender<ResourceType>& extender,
                    Resource<ResourceType>* extended_resource) override {
            auto sum_value = resource.get_value() + extender.get_value();
            sum_value = std::max(min_time_window_, sum_value);
            extended_resource->set_value(sum_value);
        }

        void extend_back(const Resource<ResourceType>& resource,
                         const Extender<ResourceType>& extender,
                         Resource<ResourceType>* extended_resource) override {
            auto sum_value = resource.get_value() + extender.get_value();
            sum_value = std::min(max_time_window_, sum_value);
            extended_resource->set_value(sum_value);
        }

    private:
        const std::map<size_t, std::pair<ValueType, ValueType>>& time_window_by_node_id_;
        ValueType min_time_window_{0};
        ValueType max_time_window_{0};

        void preprocess(size_t origin_id, size_t destination_id) override {
            min_time_window_ = time_window_by_node_id_.at(destination_id).first;  // extend
            max_time_window_ = time_window_by_node_id_.at(origin_id).second;      // extend back
        }
};
}  // namespace rcspp
