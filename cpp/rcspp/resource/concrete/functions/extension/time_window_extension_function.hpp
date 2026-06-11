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

/// @brief Extension function that enforces time-window constraints on a numerical resource.
///
/// This function is designed for vehicle routing and scheduling problems where each node
/// has a time window `[earliest, latest]`.  It extends the resource as follows:
///
/// - **Forward extension**: `max(earliest[destination], current + arc_time)` — a vehicle
///   arriving before the earliest service time waits until that time.
/// - **Backward extension**: `min(latest[origin], current + arc_time)` — used in
///   bidirectional labelling to propagate the latest permissible departure time.
///
/// The relevant time-window bounds are cached per arc via `preprocess()`.
///
/// @tparam ResourceType A NumericalResource-compatible type whose value type is arithmetic.
/// @tparam ValueType    Deduced value type of the resource (default: `ResourceType::get_value()`
///                      return type after decay).
template <typename ResourceType,
          typename ValueType = std::decay_t<decltype(std::declval<ResourceType>().get_value())>>
class TimeWindowExtensionFunction
    : public Clonable<TimeWindowExtensionFunction<ResourceType, ValueType>,
                      ExtensionFunction<ResourceType>> {
    public:
        /// @brief Constructs a TimeWindowExtensionFunction with node time windows.
        ///
        /// @param time_window_by_node_id     Map from node id to `{earliest, latest}` pair.
        ///                                   Nodes absent from the map are unconstrained.
        /// @param default_max_time_window    Fallback upper bound used when no time window is
        ///                                   defined for the origin node during backward
        ///                                   extension. Defaults to half of the value type's
        ///                                   maximum to avoid overflow when adding arc times.
        explicit TimeWindowExtensionFunction(
            std::map<size_t, std::pair<ValueType, ValueType>> time_window_by_node_id,
            ValueType default_max_time_window = std::numeric_limits<ValueType>::max() / 2)
            : time_window_by_node_id_(
                  std::make_shared<const std::map<size_t, std::pair<ValueType, ValueType>>>(
                      std::move(time_window_by_node_id))),
              max_time_window_(default_max_time_window) {}

        /// @brief Forward extension: adds arc time and clamps to the destination's earliest
        /// time.
        ///
        /// @param resource           Current time resource of the forward label.
        /// @param extender_value     Arc's travel time.
        /// @param extended_resource  Output: receives `max(earliest[dest], current + arc_time)`.
        void extend(const ResourceType& resource, const ResourceType& extender_value,
                    ResourceType* extended_resource) override {
            auto sum_value = resource.get_value() + extender_value.get_value();
            sum_value = std::max(min_time_window_, sum_value);
            extended_resource->set_value(sum_value);
        }

        /// @brief Backward extension: adds arc time and clamps to the origin's latest time.
        ///
        /// @param resource           Current time resource of the backward label.
        /// @param extender_value     Arc's travel time.
        /// @param extended_resource  Output: receives `min(latest[origin], current + arc_time)`.
        void extend_back(const ResourceType& resource, const ResourceType& extender_value,
                         ResourceType* extended_resource) override {
            auto sum_value = resource.get_value() + extender_value.get_value();
            sum_value = std::min(max_time_window_, sum_value);
            extended_resource->set_value(sum_value);
        }

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
