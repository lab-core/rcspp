// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
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
/// neither clamp binds. Both bounds come from the derived class (@c lower_bound_at,
/// @c upper_bound_at), which must state the same per-node bounds as the paired feasibility
/// function; the concrete forms take a @c SharedNodeBounds for that. A bidirectional solve refuses
/// a backward clamp that disagrees with the feasibility function.
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
        /// @return This function's upper bound there, or @c std::nullopt for no clamp.
        [[nodiscard]] auto back_ceiling_at(size_t node_id) const -> std::optional<R> override {
            if (auto upper = upper_bound_at(node_id)) {
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

        /// @brief This node's upper bound, or @c nullopt for no backward clamp.
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

        /// @brief Caches this arc's bounds: forward arrives at the destination, backward at the
        ///        origin, where its lower bound marks the deadlines no arrival can meet.
        ///
        /// @param origin_id      Index of the arc's origin node.
        /// @param destination_id Index of the arc's destination node.
        void preprocess(size_t origin_id, size_t destination_id) final {
            lower_ = lower_bound_at(destination_id);
            upper_ = upper_bound_at(origin_id);
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

/// @brief Declares a backward kind without overriding anything, for functions whose backward
///        form is their forward one.
///
/// Distinguishes "the default is right" from "never considered" (@c Unspecified). It does not
/// finalise @c extend or @c extend_back.
///
/// @warning @c BackwardKind::ArcValue asserts the arc's value is genuine per-arc data. A node
///          identity such as `{origin}` offsets the backward memory by one node; use
///          @c EndpointMirrorForm for that shape.
///
/// @tparam Base The base to insert above, normally @c ExtensionFunction<R>.
/// @tparam Kind The kind this function declares.
template <typename Base, BackwardKind Kind>
class DeclaredKindForm : public Base {
        // To declare no kind, derive from ExtensionFunction directly instead.
        static_assert(Kind != BackwardKind::Unspecified,
                      "DeclaredKindForm is for declaring a shape; to declare none, derive from "
                      "ExtensionFunction directly and let the default stand");

    public:
        static constexpr BackwardKind kind = Kind;

        [[nodiscard]] BackwardKind backward_kind() const final { return kind; }
};

/// @brief Shape base for a container resource whose backward form swaps a node identity.
///
/// The derived class writes one direction-blind formula (@c apply) over a @c Side; this form
/// builds the forward and backward sides from the arc's endpoints, so the origin/destination
/// swap is written once. The arc's value is not used.
///
/// A @c Side carries both the node left (which joins the memory) and the arrival node's
/// neighborhood (which filters it), so the stored memory is already filtered at the node it sits
/// on and two halves meeting there compare like with like.
///
/// @note A container whose arc value is genuine per-arc data is a
///       `DeclaredKindForm<ExtensionFunction<R>, BackwardKind::ArcValue>` instead. Only this form
///       gives a memory that excludes its own node in both directions.
///
/// @tparam R    The container resource type.
/// @tparam Base The base to insert above, normally @c ExtensionFunction<R>.
template <typename R, typename Base>
class EndpointMirrorForm : public Base {
    public:
        static constexpr BackwardKind kind = BackwardKind::EndpointMirror;

        [[nodiscard]] BackwardKind backward_kind() const final { return kind; }

        /// @brief Forward extension: leaves the arc's origin, arrives at its destination.
        ///
        /// @param resource          Current container resource of the forward label.
        /// @param extender_value    Arc's extender resource: unused, except by
        ///                          @ref check_arc_value in a debug build.
        /// @param extended_resource Output: receives the new container value.
        void extend(const R& resource, [[maybe_unused]] const R& extender_value,
                    R* extended_resource) final {
#ifndef NDEBUG
            check_arc_value(extender_value, forward_side_);
#endif
            apply(resource, extended_resource, forward_side_);
        }

        /// @brief Backward extension: the same formula, leaving the destination and arriving at
        ///        the origin.
        ///
        /// @param resource          Current container resource of the backward label.
        /// @param extender_value    Arc's extender resource (unused).
        /// @param extended_resource Output: receives the new container value.
        void extend_back(const R& resource, const R& /*extender_value*/,
                         R* extended_resource) final {
            apply(resource, extended_resource, backward_side_);
        }

    protected:
        /// @brief One traversal of this arc, oriented: what is left, and what is arrived at.
        struct Side {
                R node_left;             ///< the singleton {node being left}
                R arrival_neighborhood;  ///< the per-node set of the node being ARRIVED at
        };

        /// @brief Debug-build hook to inspect the arc's value, which the form never reads.
        ///
        /// Called on every forward extension when @c NDEBUG is not defined. The default accepts
        /// anything.
        ///
        /// @param extender_value The arc's value.
        /// @param side           This traversal's node left and arrival neighborhood.
        virtual void check_arc_value(const R& /*extender_value*/, const Side& /*side*/) const {}

        /// @brief The direction-blind extension formula.
        ///
        /// @param resource          The label's current value.
        /// @param extended_resource Output: receives the new value.
        /// @param side              This traversal's node left and arrival neighborhood.
        virtual void apply(const R& resource, R* extended_resource, const Side& side) const = 0;

        /// @brief Builds the side for one traversal.
        ///
        /// @param node_left_id    Index of the node the label leaves.
        /// @param node_arrived_id Index of the node the label arrives at.
        /// @return The singleton of the node left, and the neighborhood of the node arrived at.
        [[nodiscard]] virtual Side make_side(size_t node_left_id, size_t node_arrived_id) const = 0;

    private:
        Side forward_side_;
        Side backward_side_;

        /// @brief Caches both of this arc's sides: forward leaves the origin and arrives at the
        ///        destination, backward the reverse.
        ///
        /// @param origin_id      Index of the arc's origin node.
        /// @param destination_id Index of the arc's destination node.
        void preprocess(size_t origin_id, size_t destination_id) final {
            forward_side_ = make_side(origin_id, destination_id);
            backward_side_ = make_side(destination_id, origin_id);
        }
};

}  // namespace rcspp
