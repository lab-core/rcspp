// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

#include "rcspp/resource/functions/extension/backward_form.hpp"

namespace rcspp {

/// @brief Base for a bounded scalar resource read backward as a threshold: a time window, a
///        capacity.
///
/// Forward a label holds the value reached so far. Backward it holds the most a forward label may
/// have reached at the node and still finish the suffix: for time, a deadline. The join then only
/// compares the two (`forward <= backward`), and backward dominance prefers larger values.
///
/// A derived class gives the forward step and its inverse (@c forward_value, @c backward_value) and
/// the per-node bounds (@c lower_bound_at, @c upper_bound_at). This class applies the bounds:
/// forward values are raised to the lower bound (waiting); backward values are lowered to the upper
/// bound, and become @c unmeetable below the lower one; a backward label starts at its sink's upper
/// bound (@c start_back), the one bound no backward step applies. The upper bound is the paired
/// feasibility function's where it states one (see @c adopt_ceilings), else @c upper_bound_at, so
/// the two searches solve the same model.
///
/// The formulas must satisfy `extend(x, arc) <= b  <==>  x <= extend_back(b, arc)` wherever neither
/// clamp binds.
///
/// Backward kind: @c Threshold if the value type is signed, @c Unspecified if it is unsigned.
/// Going backward, a deadline can become impossible to meet: a deadline of 3 before an arc that
/// takes 5 means leaving at -2. The form marks such a label with the type's lowest value. For a
/// signed type that is a large negative number that no real value reaches. For an unsigned type
/// it is 0, which is also a real value, so an impossible label could not be told apart from a
/// valid one; such a resource is left @c Unspecified, and a bidirectional solve refuses it.
///
/// @note Use as the middle argument of a three-argument @c Clonable:
///       `Clonable<Derived, ThresholdForm<R, ExtensionFunction<R>>, ExtensionFunction<R>>`.
///
/// @tparam R    The scalar resource type.
/// @tparam Base The base to insert above -- @c ExtensionFunction<R> in every current use.
/// @tparam V    The resource's arithmetic value type, and no other.
template <typename R, typename Base,
          typename V = std::decay_t<decltype(std::declval<R>().get_value())>>
class ThresholdForm : public BackwardForm<Base, (std::is_signed_v<V> ? BackwardKind::Threshold
                                                                     : BackwardKind::Unspecified)> {
        // Both formulas convert to V and back, so any other type truncates or widens every value:
        // a real resource read through int bounds loses its fractions in every algorithm.
        static_assert(std::is_same_v<V, std::decay_t<decltype(std::declval<R>().get_value())>>,
                      "ThresholdForm: V must be the resource's own value type");

    public:
        /// @brief Forward step: @c forward_value, raised to the arrival node's lower bound
        ///        (waiting, for a time window).
        ///
        /// @param resource           Current resource of the forward label.
        /// @param extender_value     Arc's consumption.
        /// @param extended_resource  Output: receives `max(lower, forward_value(...))`.
        void extend(const R& resource, const R& extender_value, R* extended_resource) final {
            V value = forward_value(resource.get_value(), extender_value.get_value());
            if (lower_) {
                value = std::max(*lower_, value);
            }
            extended_resource->set_value(value);
        }

        /// @brief Backward step: @c backward_value, lowered to the departure node's upper bound.
        ///
        /// Never raised: a value below that node's lower bound is a deadline no forward label can
        /// meet, and becomes @c unmeetable.
        ///
        /// @param resource           Current resource of the backward label.
        /// @param extender_value     Arc's consumption.
        /// @param extended_resource  Output: receives `min(upper, backward_value(...))`, or
        ///                           `lowest()` when that is below the origin's lower bound.
        void extend_back(const R& resource, const R& extender_value, R* extended_resource) final {
            if constexpr (std::is_signed_v<V>) {
                if (resource.get_value() == unmeetable) {
                    extended_resource->set_value(unmeetable);
                    return;
                }
            }
            V value = backward_value(resource.get_value(), extender_value.get_value());
            if (upper_) {
                value = std::min(*upper_, value);
            }
            if constexpr (std::is_signed_v<V>) {
                if (back_lower_ && value < *back_lower_) {
                    value = unmeetable;
                }
            }
            extended_resource->set_value(value);
        }

        /// @brief Clamps backward to the paired feasibility function's upper bounds from now on.
        ///
        /// @param ceiling_at The feasibility function's upper bound at a node, if it has one.
        void adopt_ceilings(typename Base::CeilingSource ceiling_at) override {
            ceiling_at_ = std::move(ceiling_at);
        }

        /// @brief Backward start: the destination's upper bound, which no backward step applies,
        ///        since none arrives at a sink. Without one, the type default.
        ///
        /// @param resource Output: receives the destination's upper bound, if it has one.
        void start_back(R* resource) final {
            if (back_start_) {
                resource->set_value(*back_start_);
            }
        }

    protected:
        /// @brief The forward step before any clamp, e.g. `value + arc`.
        ///
        /// @param value The label's current value.
        /// @param arc   The arc's consumption.
        /// @return The unclamped forward value.
        [[nodiscard]] virtual V forward_value(V value, V arc) const = 0;

        /// @brief The backward step before any clamp: the largest `x` with
        ///        `forward_value(x, arc) <= value`.
        ///
        /// @param value The label's current bound.
        /// @param arc   The arc's consumption.
        /// @return The unclamped backward value.
        [[nodiscard]] virtual V backward_value(V value, V arc) const = 0;

        /// @brief This node's lower bound: forward values are raised to it, backward values below
        ///        it are unmeetable.
        ///
        /// @c nullopt when there is none (a capacity does not wait).
        ///
        /// @param node_id Index of the node the extension arrives at.
        /// @return The bound to clamp up to, or @c nullopt.
        [[nodiscard]] virtual std::optional<V> lower_bound_at(size_t node_id) const = 0;

        /// @brief This node's upper bound: backward values are lowered to it, and a backward label
        ///        starting here starts at it.
        ///
        /// Used only where the paired feasibility function states no bound of its own.
        ///
        /// @param node_id Index of the node the extension arrives at.
        /// @return The bound to clamp down to, or @c nullopt.
        [[nodiscard]] virtual std::optional<V> upper_bound_at(size_t node_id) const = 0;

    private:
        /// @brief The backward value of a deadline no forward label can meet. It propagates
        ///        unchanged and fails every backward test.
        static constexpr V unmeetable = std::numeric_limits<V>::lowest();

        /// @brief Bounds of the arc being extended, cached by @c preprocess: the arrival's lower
        ///        bound (forward), the departure's upper and lower bounds (backward), and the
        ///        arrival's upper bound, where a backward label starting there starts.
        std::optional<V> lower_;
        std::optional<V> upper_;
        std::optional<V> back_lower_;
        std::optional<V> back_start_;
        typename Base::CeilingSource ceiling_at_;

        /// @brief The feasibility function's bound at a node, else this function's own
        ///        (@c upper_bound_at): the backward clamp there, and the backward start at a sink.
        ///
        /// @param node_id Index of the node the backward extension arrives at.
        /// @return The feasibility function's bound there, else this function's own.
        [[nodiscard]] std::optional<V> backward_bound_at(size_t node_id) const {
            if (ceiling_at_) {
                if (auto ceiling = ceiling_at_(node_id)) {
                    return static_cast<V>(ceiling->get_value());
                }
            }
            return upper_bound_at(node_id);
        }

        /// @brief Caches this arc's bounds: forward arrives at the destination, backward at the
        ///        origin, where its lower bound marks the deadlines no arrival can meet. A backward
        ///        label starting at the destination starts at its upper bound.
        ///
        /// @param origin_id      Index of the arc's origin node.
        /// @param destination_id Index of the arc's destination node.
        void preprocess(size_t origin_id, size_t destination_id) final {
            lower_ = lower_bound_at(destination_id);
            upper_ = backward_bound_at(origin_id);
            back_lower_ = lower_bound_at(origin_id);
            back_start_ = backward_bound_at(destination_id);
        }
};

/// @brief A @ref ThresholdForm for additive resources: forward adds the arc's consumption, backward
///        subtracts it.
///
/// Time windows and capacities are built on it.
///
/// @tparam R    The scalar resource type.
/// @tparam Base The base to insert above -- @c ExtensionFunction<R> in every current use.
/// @tparam V    The resource's arithmetic value type.
template <typename R, typename Base,
          typename V = std::decay_t<decltype(std::declval<R>().get_value())>>
class TranslationThresholdForm : public ThresholdForm<R, Base, V> {
    protected:
        /// @brief Adds the arc's consumption.
        ///
        /// @param value The label's current value.
        /// @param arc   The arc's consumption.
        /// @return `value + arc`.
        [[nodiscard]] V forward_value(V value, V arc) const final { return value + arc; }

        /// @brief Subtracts the arc's consumption, saturating at zero for an unsigned type.
        ///
        /// @param value The label's current bound.
        /// @param arc   The arc's consumption.
        /// @return `value - arc`, or `0` where an unsigned subtraction would underflow.
        [[nodiscard]] V backward_value(V value, V arc) const final {
            if constexpr (std::is_signed_v<V>) {
                return value - arc;
            } else {
                return value < arc ? V{0} : value - arc;
            }
        }
};

}  // namespace rcspp
