// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"
#include "rcspp/resource/functions/node_bounds.hpp"

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
/// Build it and the paired @c TimeWindowExtensionFunction from one @ref SharedNodeBounds (see
/// @ref bounds), as @c presets::add_window_resource does.
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
            : TimeWindowFeasibilityFunction(make_node_bounds(ValueType{0}, default_max_time_window,
                                                             std::move(time_window_by_node_id))) {}

        /// @brief Constructs the function on shared per-node `{opening, closing}` windows.
        ///
        /// Pass the same object to the paired @c TimeWindowExtensionFunction, or read it back
        /// through @ref bounds.
        ///
        /// @param windows The windows; must not be null.
        /// @throws std::invalid_argument If @p windows is null.
        explicit TimeWindowFeasibilityFunction(SharedNodeBounds<ValueType> windows)
            : windows_(std::move(windows)) {
            if (windows_ == nullptr) {
                throw std::invalid_argument(
                    "TimeWindowFeasibilityFunction: windows must not be null");
            }
            min_time_window_ = windows_->default_lower();
            max_time_window_ = windows_->default_upper();
        }

        /// @brief The per-node windows this function enforces, to share with the paired
        ///        extension function.
        ///
        /// @return The shared windows.
        [[nodiscard]] auto bounds() const -> const SharedNodeBounds<ValueType>& { return windows_; }

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

        /// @brief A backward label holds the deadline directly, so the merge test is
        ///        `arrival <= deadline`.
        ///
        /// Evaluated on the raw values, so a relaxed dominance never lets a late arrival join.
        ///
        /// @return @c MergeRule::DominanceOrder.
        [[nodiscard]] MergeRule merge_rule() const override { return MergeRule::DominanceOrder; }

        /// @brief A backward label at this node starts at the node's closing time.
        ///
        /// Uses the bound cached by `preprocess(node_id)`, so sinks can seed differently.
        ///
        /// @return The node's upper time bound, as the seed for a backward label here.
        [[nodiscard]] auto back_seed_value() const -> std::optional<ResourceType> override {
            ResourceType seed;
            seed.set_value(max_time_window_);
            return seed;
        }

        /// @brief This node's opening time, which @ref is_back_feasible tests.
        ///
        /// @param node_id Index of the node.
        /// @return The node's lower time bound.
        [[nodiscard]] auto back_floor_at(size_t node_id) const
            -> std::optional<ResourceType> override {
            ResourceType floor;
            floor.set_value(windows_->lower(node_id));
            return floor;
        }

    private:
        SharedNodeBounds<ValueType> windows_;
        ValueType min_time_window_{0};
        ValueType max_time_window_{};

        void preprocess(size_t node_id) override {
            std::tie(min_time_window_, max_time_window_) = windows_->at(node_id);
        }
};

/// A time window seeds backward labels at the node's closing time (a ceiling). Both template
/// parameters must be named, or the specialisation never matches.
template <typename R, typename V>
struct BackSeedEndOf<TimeWindowFeasibilityFunction<R, V>> {
        static constexpr BackSeedEnd value = BackSeedEnd::Ceiling;
};

}  // namespace rcspp
