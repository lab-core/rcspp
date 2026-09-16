// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/extension/backward_form.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

/// @brief Extension function that enforces time-window constraints on a numerical resource.
///
/// This function is designed for vehicle routing and scheduling problems where each node
/// has a time window `[earliest, latest]`.  It extends the resource as follows:
///
/// - **Forward extension**: `max(earliest[destination], current + arc_time)` — a vehicle
///   arriving before the earliest service time waits until that time.
/// - **Backward extension**: `min(latest[origin], current - arc_time)` — used in
///   bidirectional labelling to propagate the latest permissible departure time. A backward
///   label stores a *deadline*, so the arc time is subtracted, not added.
///
/// Both formulas, and the discipline that decides which clamp binds in which direction, come
/// from `TranslationThresholdForm`. This class only answers *what the bounds are*: a window is a
/// floor and a ceiling at every node, which is the single thing that distinguishes it from
/// `BudgetExtensionFunction`. The form caches both per arc through `preprocess()`.
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
            : time_window_by_node_id_(
                  std::make_shared<const std::map<size_t, std::pair<ValueType, ValueType>>>(
                      std::move(time_window_by_node_id))),
              default_max_time_window_(default_max_time_window) {}

    protected:
        /// @brief The node's opening time, defaulting to zero.
        ///
        /// Absent from the map means `0`, not "no clamp": the forward extension has always
        /// clamped up to zero, and a resource whose value can go negative through a reduced cost
        /// would notice the difference.
        ///
        /// @param node_id Index of the node the forward extension arrives at.
        /// @return The node's earliest service time, or zero if it has no window.
        [[nodiscard]] std::optional<ValueType> lower_bound_at(size_t node_id) const final {
            auto it = time_window_by_node_id_->find(node_id);
            return it != time_window_by_node_id_->end() ? it->second.first
                                                        : default_min_time_window_;
        }

        /// @brief The node's closing time, defaulting to the constructor's bound.
        ///
        /// That default is `max()/2`, so the forward addition cannot overflow. There is
        /// deliberately no clamp *up* to the opening time on the backward side:
        /// `TimeWindowFeasibilityFunction::is_back_feasible` tests `value >= earliest`, and
        /// clamping up to that bound would make every infeasible backward label look feasible.
        /// The form owns that rule.
        ///
        /// @param node_id Index of the node the backward extension arrives at.
        /// @return The node's latest service time, or the default bound if it has no window.
        [[nodiscard]] std::optional<ValueType> upper_bound_at(size_t node_id) const final {
            auto it = time_window_by_node_id_->find(node_id);
            return it != time_window_by_node_id_->end() ? it->second.second
                                                        : default_max_time_window_;
        }

    private:
        std::shared_ptr<const std::map<size_t, std::pair<ValueType, ValueType>>>
            time_window_by_node_id_;
        ValueType default_min_time_window_{0};
        ValueType default_max_time_window_;
};
}  // namespace rcspp
