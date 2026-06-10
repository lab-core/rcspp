// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename ResourceType,
          typename ValueType = std::decay_t<decltype(std::declval<ResourceType>().get_value())>>
class TimeWindowExtensionFunction
    : public Clonable<TimeWindowExtensionFunction<ResourceType, ValueType>,
                      ExtensionFunction<ResourceType>> {
    public:
        explicit TimeWindowExtensionFunction(
            std::map<size_t, std::pair<ValueType, ValueType>> time_window_by_node_id,
            ValueType default_max_time_window = std::numeric_limits<ValueType>::max() / 2)
            : time_window_by_node_id_(
                  std::make_shared<const std::map<size_t, std::pair<ValueType, ValueType>>>(
                      std::move(time_window_by_node_id))),
              max_time_window_(default_max_time_window) {}

        void extend(const ResourceType& resource, const ResourceType& extender_value,
                    ResourceType* extended_resource) override {
            auto sum_value = resource.get_value() + extender_value.get_value();
            sum_value = std::max(min_time_window_, sum_value);
            extended_resource->set_value(sum_value);
        }

        // GCOVR_EXCL_START (extend_back for bidirectional search; not called in unit tests)
        void extend_back(const ResourceType& resource, const ResourceType& extender_value,
                         ResourceType* extended_resource) override {
            auto sum_value = resource.get_value() + extender_value.get_value();
            sum_value = std::min(max_time_window_, sum_value);
            extended_resource->set_value(sum_value);
        }
        // GCOVR_EXCL_STOP

    private:
        std::shared_ptr<const std::map<size_t, std::pair<ValueType, ValueType>>>
            time_window_by_node_id_;
        ValueType min_time_window_{0};
        ValueType max_time_window_;

        void preprocess(size_t origin_id, size_t destination_id) override {
            auto it = time_window_by_node_id_->find(destination_id);
            if (it != time_window_by_node_id_->end()) {
                min_time_window_ = it->second.first;
            }
            it = time_window_by_node_id_->find(origin_id);
            if (it != time_window_by_node_id_->end()) {
                max_time_window_ = it->second.second;
            }
        }
};
}  // namespace rcspp
