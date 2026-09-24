// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/extension/backward_form.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"
#include "rcspp/resource/functions/node_bounds.hpp"

namespace rcspp {

/// @brief Extension function that enforces time-window constraints on a numerical resource.
///
/// This function is designed for vehicle routing and scheduling problems where each node
/// has a time window `[earliest, latest]`.  It extends the resource as follows:
///
/// - **Forward extension**: `max(earliest[destination], current + arc_time)` — a vehicle
///   arriving before the earliest service time waits until that time.
/// - **Backward extension**: `min(latest[origin], current - arc_time)` — a backward label
///   stores the latest permissible departure time (a deadline).
///
/// The formulas come from `TranslationThresholdForm`; this class only supplies the per-node
/// floor and ceiling. They must be the windows the paired feasibility function enforces: build
/// both from one @ref SharedNodeBounds, as @c presets::add_window_resource does.
///
/// @tparam ResourceType A NumericalResource-compatible type whose value type is arithmetic.
/// @tparam ValueType    Deduced value type of the resource (default: `ResourceType::get_value()`
///                      return type after decay).
template <typename ResourceType,
          typename ValueType = std::decay_t<decltype(std::declval<ResourceType>().get_value())>>
class TimeWindowExtensionFunction
    : public Clonable<
          TimeWindowExtensionFunction<ResourceType, ValueType>,
          TranslationThresholdForm<ResourceType, ExtensionFunction<ResourceType>, ValueType>,
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
            : TimeWindowExtensionFunction(make_node_bounds(ValueType{0}, default_max_time_window,
                                                           std::move(time_window_by_node_id))) {}

        /// @brief Constructs the function on shared per-node `{earliest, latest}` windows.
        ///
        /// @param windows The windows, shared with the paired feasibility function; must not be
        ///                null.
        /// @throws std::invalid_argument If @p windows is null.
        explicit TimeWindowExtensionFunction(SharedNodeBounds<ValueType> windows)
            : windows_(std::move(windows)) {
            if (windows_ == nullptr) {
                throw std::invalid_argument(
                    "TimeWindowExtensionFunction: windows must not be null");
            }
        }

        /// @brief The per-node windows this function waits for and clamps to.
        ///
        /// @return The shared windows.
        [[nodiscard]] auto bounds() const -> const SharedNodeBounds<ValueType>& { return windows_; }

    protected:
        /// @brief The node's opening time, defaulting to zero.
        ///
        /// A node absent from the map clamps to `0`, not "no clamp".
        ///
        /// @param node_id Index of the node the forward extension arrives at.
        /// @return The node's earliest service time, or zero if it has no window.
        [[nodiscard]] std::optional<ValueType> lower_bound_at(size_t node_id) const final {
            return windows_->lower(node_id);
        }

        /// @brief The node's closing time, defaulting to the constructor's bound.
        ///
        /// Backward values are deliberately not clamped up to the opening time, or infeasible
        /// backward labels would look feasible.
        ///
        /// @param node_id Index of the node the backward extension arrives at.
        /// @return The node's latest service time, or the default bound if it has no window.
        [[nodiscard]] std::optional<ValueType> upper_bound_at(size_t node_id) const final {
            return windows_->upper(node_id);
        }

    private:
        SharedNodeBounds<ValueType> windows_;
};
}  // namespace rcspp
