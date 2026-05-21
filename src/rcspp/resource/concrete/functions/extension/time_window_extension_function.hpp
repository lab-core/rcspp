// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <map>
#include <memory>
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
        explicit TimeWindowExtensionFunction(std::map<size_t, ValueType> min_time_window_by_dest_id)
            : min_time_window_by_dest_id_(std::make_shared<const std::map<size_t, ValueType>>(
                  std::move(min_time_window_by_dest_id))) {}

        void extend(const Resource<ResourceType>& resource, const Extender<ResourceType>& extender,
                    Resource<ResourceType>* extended_resource) override {
            auto sum_value = resource.get_value() + extender.get_value();
            sum_value = std::max(min_time_window_, sum_value);
            extended_resource->set_value(sum_value);
        }

    private:
        std::shared_ptr<const std::map<size_t, ValueType>> min_time_window_by_dest_id_;
        ValueType min_time_window_{0};

        void preprocess(size_t /* origin_id */, size_t destination_id) override {
            if (min_time_window_by_dest_id_ == nullptr) {
                return;
            }
            auto it = min_time_window_by_dest_id_->find(destination_id);
            // if found, update min_time_window_
            // else, keep min_time_window_ to it's initial value (0 by default)
            if (it != min_time_window_by_dest_id_->end()) {
                min_time_window_ = it->second;
            }
        }
};
}  // namespace rcspp
