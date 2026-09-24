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

#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

/// @brief Base for a resource that stores a bound (e.g. a time window) rather than an
///        accumulation. A derived class supplies two scalar formulas and two node bound lookups.
///
/// Forward clamps up to the node's lower bound (waiting); backward clamps down to the node's
/// upper bound and is never clamped up, so infeasible backward labels stay detectable. A backward
/// value below the node's lower bound is a deadline no forward arrival can meet, so it becomes
/// `numeric_limits<V>::lowest()`, which stays there.
/// The formulas must satisfy `extend(x, arc) <= b  <==>  x <= extend_back(b, arc)` wherever
/// neither clamp binds. The backward clamp uses the paired feasibility function's upper bound
/// (see @c adopt_ceilings), falling back to @c upper_bound_at.
///
/// @note Use as the middle argument of a three-argument @c Clonable:
///       `Clonable<Derived, ThresholdForm<R, ExtensionFunction<R>>, ExtensionFunction<R>>`.
///
/// @tparam R    The scalar resource type.
/// @tparam Base The base to insert above -- @c ExtensionFunction<R> in every current use.
/// @tparam V    The resource's arithmetic value type, and no other.
template <typename R, typename Base,
          typename V = std::decay_t<decltype(std::declval<R>().get_value())>>
class ThresholdForm : public Base {
        // Both formulas convert to V and back, so any other type truncates or widens every value:
        // a real resource read through int bounds loses its fractions in every algorithm.
        static_assert(std::is_same_v<V, std::decay_t<decltype(std::declval<R>().get_value())>>,
                      "ThresholdForm: V must be the resource's own value type");

    public:
        /// @brief @c Threshold for a signed value type; @c Unspecified for an unsigned one, which
        ///        cannot represent an unmeetable backward bound (bidirectional then refuses).
        static constexpr BackwardKind kind =
            std::is_signed_v<V> ? BackwardKind::Threshold : BackwardKind::Unspecified;

        [[nodiscard]] BackwardKind backward_kind() const final { return kind; }

        /// @brief Forward extension: the derived formula, clamped up to this node's lower bound.
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

        /// @brief Backward extension: the inverted formula, clamped down to this node's upper
        ///        bound (never up to its lower one).
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

        /// @brief This node's lower bound, the value a forward extension is clamped up to.
        ///
        /// @param node_id Index of the node.
        /// @return The lower bound, or @c std::nullopt for no forward clamp.
        [[nodiscard]] auto floor_at(size_t node_id) const -> std::optional<R> override {
            if (auto lower = lower_bound_at(node_id)) {
                R floor;
                floor.set_value(*lower);
                return floor;
            }
            return std::nullopt;
        }

        /// @brief The bound a backward extension arriving at @p node_id clamps down to.
        ///
        /// @param node_id Index of the node.
        /// @return The feasibility function's ceiling there, else this function's upper bound.
        [[nodiscard]] auto back_ceiling_at(size_t node_id) const -> std::optional<R> override {
            if (auto upper = backward_bound_at(node_id)) {
                R ceiling;
                ceiling.set_value(*upper);
                return ceiling;
            }
            return std::nullopt;
        }

    protected:
        /// @brief The forward step, before any clamp. Typically `value + arc`.
        ///
        /// @param value The label's current value.
        /// @param arc   The arc's consumption.
        /// @return The unclamped forward value.
        [[nodiscard]] virtual V forward_value(V value, V arc) const = 0;

        /// @brief The backward step, before any clamp. Must invert @c forward_value.
        ///
        /// @param value The label's current bound.
        /// @param arc   The arc's consumption.
        /// @return The unclamped backward value.
        [[nodiscard]] virtual V backward_value(V value, V arc) const = 0;

        /// @brief This node's lower bound, or @c nullopt for no forward clamp.
        ///
        /// @param node_id Index of the node the extension arrives at.
        /// @return The bound to clamp up to, or @c nullopt.
        [[nodiscard]] virtual std::optional<V> lower_bound_at(size_t node_id) const = 0;

        /// @brief This node's upper bound, or @c nullopt for no backward clamp. Used only where
        ///        the paired feasibility function states no bound of its own.
        ///
        /// @param node_id Index of the node the extension arrives at.
        /// @return The bound to clamp down to, or @c nullopt.
        [[nodiscard]] virtual std::optional<V> upper_bound_at(size_t node_id) const = 0;

    private:
        /// @brief A backward value no forward arrival can meet.
        static constexpr V unmeetable = std::numeric_limits<V>::lowest();

        std::optional<V> lower_;
        std::optional<V> upper_;
        std::optional<V> back_lower_;
        typename Base::CeilingSource ceiling_at_;

        /// @brief The bound a backward extension arriving at @p node_id clamps down to.
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
        ///        origin, where its lower bound marks the deadlines no arrival can meet.
        ///
        /// @param origin_id      Index of the arc's origin node.
        /// @param destination_id Index of the arc's destination node.
        void preprocess(size_t origin_id, size_t destination_id) final {
            lower_ = lower_bound_at(destination_id);
            upper_ = backward_bound_at(origin_id);
            back_lower_ = lower_bound_at(origin_id);
        }
};

/// @brief A @ref ThresholdForm whose forward step adds the arc's consumption and whose backward
///        step subtracts it (saturating at zero for an unsigned type, rather than wrapping).
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
