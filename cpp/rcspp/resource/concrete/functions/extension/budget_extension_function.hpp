// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <type_traits>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

/// @brief Additive forward, threshold backward: for capacity- and duration-like resources.
///
/// Forward this is the same accumulation as `AdditionExtensionFunction`. Backward it inverts:
/// `b(u) = min(max_at_node, b(v) - q)`. The clamp is by *this node's upper bound* and is what
/// makes a limit in the middle of the path propagate backwards; there is deliberately no clamp
/// from below, because `MinMaxFeasibilityFunction::is_back_feasible` is what must reject an
/// infeasible result.
///
/// This is a separate class from `AdditionExtensionFunction` because the two agree forward and
/// diverge backward: cost *accumulates* and so must keep adding in both directions, while a
/// capacity is a *threshold* and must subtract. `AdditionExtensionFunction` is the cost slot's
/// extension function in every example and binding, so it keeps inheriting the default
/// `extend_back`.
///
/// @note Known wart: the per-node bounds duplicate the map that `MinMaxFeasibilityFunction`
///       also holds. That is an existing pattern in this library -- `TimeWindowExtensionFunction`
///       and `TimeWindowFeasibilityFunction` are already constructed with the same window map --
///       so it is consistent rather than novel. Making the map optional keeps the common
///       uniform-capacity case free of it entirely.
///
/// @tparam ResourceType A NumericalResource-compatible type whose value type is arithmetic.
/// @tparam ValueType    Deduced value type of the resource (default: `ResourceType::get_value()`
///                      return type after decay).
template <typename ResourceType,
          typename ValueType = std::decay_t<decltype(std::declval<ResourceType>().get_value())>>
class BudgetExtensionFunction : public Clonable<BudgetExtensionFunction<ResourceType, ValueType>,
                                                ExtensionFunction<ResourceType>> {
        // Backward extension subtracts the arc consumption, so the value type must be able to go
        // negative; an unsigned type would wrap to a huge positive value that reads as a very
        // loose budget. Same hazard, and same guard, as TimeWindowExtensionFunction.
        static_assert(std::is_signed_v<ValueType>,
                      "BudgetExtensionFunction requires a signed value type");

    public:
        /// @brief Constructs a BudgetExtensionFunction with optional per-node upper bounds.
        ///
        /// @param max_by_node_id Per-node upper bounds. May be empty: with a *uniform* capacity
        ///                       the clamp never binds (`b(v) <= Q` implies `b(v) - q <= Q`), so
        ///                       the map is only needed when per-node limits exist.
        /// @param default_max    Bound used at nodes absent from the map. Defaults to half of the
        ///                       value type's maximum so the forward addition cannot overflow.
        explicit BudgetExtensionFunction(
            std::map<size_t, ValueType> max_by_node_id = {},
            ValueType default_max = std::numeric_limits<ValueType>::max() / 2)
            : max_by_node_id_(max_by_node_id.empty()
                                  ? nullptr
                                  : std::make_shared<const std::map<size_t, ValueType>>(
                                        std::move(max_by_node_id))),
              default_max_(default_max),
              max_at_node_(default_max) {}

        /// @brief Forward extension: accumulates the arc's consumption.
        ///
        /// @param resource           Current budget resource of the forward label.
        /// @param extender_value     Arc's consumption.
        /// @param extended_resource  Output: receives `current + arc_consumption`.
        void extend(const ResourceType& resource, const ResourceType& extender_value,
                    ResourceType* extended_resource) override {
            extended_resource->set_value(resource.get_value() + extender_value.get_value());
        }

        /// @brief Backward extension: inverts the accumulation and clamps by this node's bound.
        ///
        /// @param resource           Current budget resource of the backward label.
        /// @param extender_value     Arc's consumption.
        /// @param extended_resource  Output: receives `min(max_at_node, current - consumption)`.
        void extend_back(const ResourceType& resource, const ResourceType& extender_value,
                         ResourceType* extended_resource) override {
            extended_resource->set_value(
                std::min(max_at_node_, resource.get_value() - extender_value.get_value()));
        }

        /// @brief A budget is a ceiling-style bound, so the backward form inverts and clamps.
        ///
        /// @return @c BackwardKind::Threshold.
        [[nodiscard]] BackwardKind backward_kind() const override {
            return BackwardKind::Threshold;
        }

    private:
        std::shared_ptr<const std::map<size_t, ValueType>> max_by_node_id_;
        ValueType default_max_;
        ValueType max_at_node_;

        /// @brief Caches the upper bound of the node a backward extension lands on.
        ///
        /// Backward extension lands on the arc's **origin**, so that is the node whose bound
        /// clamps.
        ///
        /// @param origin_id      Index of the arc's origin node.
        /// @param destination_id Index of the arc's destination node (unused).
        void preprocess(size_t origin_id, size_t /*destination_id*/) override {
            max_at_node_ = default_max_;
            if (max_by_node_id_ == nullptr) {
                return;
            }
            if (auto it = max_by_node_id_->find(origin_id); it != max_by_node_id_->end()) {
                max_at_node_ = it->second;
            }
        }
};
}  // namespace rcspp
