// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <map>
#include <memory>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

/// @brief Feasibility function that enforces per-node time-window constraints on a scalar
///        resource.
///
/// Each node may have an associated time window `[min_time_window, max_time_window]`.
/// A forward label is feasible when `resource.value <= max_time_window_`.
/// A backward label is back-feasible when `resource.value >= min_time_window_`.
/// Two labels can be merged when `forward.value <= backward.value`.
///
/// Nodes without an explicit entry in the map fall back to
/// `[0, default_max_time_window]`.  The default upper bound is set to
/// `numeric_limits<ValueType>::max() / 2` to prevent overflow.
///
/// @tparam ResourceType The resource type whose value supports `get_value()`, `leq()`,
///         and `geq()`.
/// @tparam ValueType The scalar type of the time value; deduced from
///         `ResourceType::get_value()`.
template <typename ResourceType,
          typename ValueType = std::decay_t<decltype(std::declval<ResourceType>().get_value())>>
class TimeWindowFeasibilityFunction
    : public Clonable<TimeWindowFeasibilityFunction<ResourceType, ValueType>,
                      FeasibilityFunction<ResourceType>> {
    public:
        /// @brief Constructs the function with per-node time-window overrides and a global
        ///        default upper bound.
        ///
        /// @param time_window_by_node_id Map from node id to a `{min, max}` time-window pair.
        /// @param default_max_time_window Upper bound applied at nodes not present in the map.
        ///        Defaults to `numeric_limits<ValueType>::max() / 2` to prevent overflow.
        explicit TimeWindowFeasibilityFunction(
            std::map<size_t, std::pair<ValueType, ValueType>> time_window_by_node_id,
            ValueType default_max_time_window = std::numeric_limits<ValueType>::max() /
                                                2)  // prevent overflow
            : time_window_by_node_id_(
                  std::make_shared<const std::map<size_t, std::pair<ValueType, ValueType>>>(
                      std::move(time_window_by_node_id))),
              default_max_time_window_(default_max_time_window),
              max_time_window_(default_max_time_window) {}

        /// @brief Checks that the forward-label resource does not exceed the node's upper time
        ///        bound.
        ///
        /// @param resource The forward-label resource to evaluate.
        /// @return `true` if `resource.value <= max_time_window_`.
        [[nodiscard]] auto is_feasible(const ResourceType& resource) -> bool override {
            return resource.get_value() <= max_time_window_;
        }

        /// @brief Checks that the backward-label resource is at least the node's lower time
        ///        bound.
        ///
        /// @param resource The backward-label resource to evaluate.
        /// @return `true` if `resource.value >= min_time_window_`.
        [[nodiscard]] auto is_back_feasible(const ResourceType& resource) -> bool override {
            return resource.get_value() >= min_time_window_;
        }

        /// @brief Checks whether a forward and a backward label can be merged.
        ///
        /// Merging is valid when the forward value does not exceed the backward value,
        /// ensuring the combined path respects non-decreasing time ordering.
        ///
        /// @param resource The forward-label resource at the merge node.
        /// @param back_resource The backward-label resource at the merge node.
        /// @return `true` if `resource.value <= back_resource.value`.
        [[nodiscard]] auto can_be_merged(const ResourceType& resource,
                                         const ResourceType& back_resource) -> bool override {
            return resource.get_value() <= back_resource.get_value();
        }

    private:
        std::shared_ptr<const std::map<size_t, std::pair<ValueType, ValueType>>>
            time_window_by_node_id_;
        ValueType default_min_time_window_{0};
        ValueType default_max_time_window_{};
        ValueType min_time_window_{0};
        ValueType max_time_window_{};

        void preprocess(size_t node_id) override {
            auto it = time_window_by_node_id_->find(node_id);
            if (it != time_window_by_node_id_->end()) {
                min_time_window_ = it->second.first;
                max_time_window_ = it->second.second;
            } else {
                min_time_window_ = default_min_time_window_;
                max_time_window_ = default_max_time_window_;
            }
        }
};
}  // namespace rcspp
