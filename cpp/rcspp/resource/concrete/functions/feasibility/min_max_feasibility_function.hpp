// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>
#include <memory>
#include <type_traits>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

/// @brief Feasibility function that enforces a [min, max] bound on a scalar resource value,
///        with optional per-node override bounds.
///
/// At each graph node the active window [min_, max_] is either the global default or the
/// per-node override supplied at construction.  The resource is feasible when
/// `min_ <= resource.value <= max_`.  A `can_be_merged` check determines whether a
/// forward and backward label can be combined during bidirectional search, using either
/// increasing or decreasing value order.
///
/// @tparam ResourceType The resource type whose value supports `geq()`, `leq()`, and
///         `get_value()`.
/// @tparam ValueType The scalar type of the resource value; deduced from
///         `ResourceType::get_value()`.
template <typename ResourceType,
          typename ValueType = std::decay_t<decltype(std::declval<ResourceType>().get_value())>>
class MinMaxFeasibilityFunction
    : public Clonable<MinMaxFeasibilityFunction<ResourceType, ValueType>,
                      FeasibilityFunction<ResourceType>> {
    public:
        /// @brief Constructs the function with a single global [min, max] window and a merge
        ///        direction flag.
        ///
        /// No per-node overrides; every node uses the same bounds.
        ///
        /// @param min Global lower bound on the resource value.
        /// @param max Global upper bound on the resource value.
        /// @param merge_by_increasing_value If `true`, merging requires
        ///        `resource.value <= back_resource.value`; if `false`, the opposite.
        MinMaxFeasibilityFunction(ValueType min, ValueType max, bool merge_by_increasing_value)
            : default_min_(min),
              default_max_(max),
              min_(min),
              max_(max),
              merge_by_increasing_value_(merge_by_increasing_value) {}

        /// @brief Constructs the function with default global bounds and optional per-node
        ///        overrides.
        ///
        /// When `min_max_by_node_id` is empty, every node uses `[default_min, default_max]`.
        ///
        /// @param default_min Default lower bound applied at nodes without a specific override.
        /// @param default_max Default upper bound applied at nodes without a specific override.
        /// @param min_max_by_node_id Map from node id to a `{min, max}` pair that overrides the
        ///        default for that node.
        MinMaxFeasibilityFunction(
            ValueType default_min, ValueType default_max,
            std::map<size_t, std::pair<ValueType, ValueType>> min_max_by_node_id = {})
            : min_max_by_node_id_(
                  min_max_by_node_id.empty()
                      ? nullptr
                      : std::make_shared<const std::map<size_t, std::pair<ValueType, ValueType>>>(
                            std::move(min_max_by_node_id))),
              default_min_(default_min),
              default_max_(default_max),
              min_(default_min),
              max_(default_max) {}

        /// @brief Checks that the resource value lies within [min_, max_].
        ///
        /// @param resource The resource to evaluate.
        /// @return `true` if the resource value satisfies both the lower and upper bound.
        [[nodiscard]] auto is_feasible(const ResourceType& resource) -> bool override {
            return resource.geq(min_) && resource.leq(max_);
        }

        /// @brief Checks whether a forward resource and a backward resource can be merged in
        ///        bidirectional search.
        ///
        /// The direction of comparison (increasing vs. decreasing) is set at construction.
        ///
        /// @param resource The forward-label resource at the merge node.
        /// @param back_resource The backward-label resource at the merge node.
        /// @return `true` if the two labels can be combined.
        [[nodiscard]] auto can_be_merged(const ResourceType& resource,
                                         const ResourceType& back_resource) -> bool override {
            if (merge_by_increasing_value_) {
                return resource.get_value() <= back_resource.get_value();
            }
            return resource.get_value() >= back_resource.get_value();
        }

    private:
        std::shared_ptr<const std::map<size_t, std::pair<ValueType, ValueType>>>
            min_max_by_node_id_;
        ValueType default_min_{};
        ValueType default_max_{};
        ValueType min_;
        ValueType max_;

        // true: merge by increasing value, false: decreasing value
        // increasing value means that resource.get_value() <= back_resource.get_value()
        bool merge_by_increasing_value_ = true;

        void preprocess(size_t node_id) override {
            if (min_max_by_node_id_ == nullptr) {
                return;
            }
            auto it = min_max_by_node_id_->find(node_id);
            if (it != min_max_by_node_id_->end()) {
                min_ = it->second.first;
                max_ = it->second.second;
            } else {
                min_ = default_min_;
                max_ = default_max_;
            }
        }
};
}  // namespace rcspp
