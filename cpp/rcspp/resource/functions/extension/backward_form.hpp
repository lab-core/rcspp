// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>

#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

/// @brief The shape base for a resource that stores a *bound* rather than an accumulation.
///
/// A derived class supplies two scalar formulas and two node -> bound lookups. It never sees a
/// direction, and it cannot reintroduce either of the two defects this library actually had: the
/// sign of the backward step and the identity of the node each clamp reads are both decided here.
///
/// **The clamp discipline, stated once.**
///  - Forward clamps *up* to this node's lower bound: arriving early means waiting.
///  - Backward clamps *down* to this node's upper bound: that is how a limit in the middle of the
///    path propagates backwards.
///  - Backward is deliberately **never** clamped up by the lower bound. Clamping up to an opening
///    time would make every infeasible backward label look feasible, robbing
///    @c is_back_feasible of the case it exists to catch.
///
/// **The correctness condition** for the pair of formulas is a definition, not a property:
/// `extend(x, arc) <= b  <==>  x <= extend_back(b, arc)`, over any range where neither clamp
/// binds. @c test_util::check_backward_contract asserts it.
///
/// @note This form is the *middle* argument of a three-argument @c Clonable: a derived class
///       writes `Clonable<Derived, ThresholdForm<R, ExtensionFunction<R>>, ExtensionFunction<R>>`
///       so that @c clone() keeps returning `unique_ptr<ExtensionFunction<R>>`. A mistake in the
///       first argument clones the wrong type; a mistake in the third changes @c clone()'s return
///       type, and the resulting error names @c ResourceFactory rather than this file.
///
/// @tparam R    The scalar resource type.
/// @tparam Base The base to insert above -- @c ExtensionFunction<R> in every current use.
/// @tparam V    The resource's arithmetic value type.
template <typename R, typename Base,
          typename V = std::decay_t<decltype(std::declval<R>().get_value())>>
class ThresholdForm : public Base {
    public:
        /// @brief A bound-style resource inverts its backward extension -- but only when the
        ///        value type can represent the inversion.
        ///
        /// Backward extension subtracts, so an unsigned type would wrap to a huge positive value
        /// that reads as a very loose bound. Saturating at zero does not rescue it either: a
        /// backward value of 0 is accepted wherever the node's lower bound is 0, so a genuinely
        /// impossible half-path would look feasible. An unsigned value type simply cannot
        /// represent "this bound cannot be met", so it declines to declare and a bidirectional
        /// solve refuses to start on it. Forward-only use is unaffected.
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
        ///        bound -- and deliberately never up by its lower one.
        ///
        /// @param resource           Current resource of the backward label.
        /// @param extender_value     Arc's consumption.
        /// @param extended_resource  Output: receives `min(upper, backward_value(...))`.
        void extend_back(const R& resource, const R& extender_value, R* extended_resource) final {
            V value = backward_value(resource.get_value(), extender_value.get_value());
            if (upper_) {
                value = std::min(*upper_, value);
            }
            extended_resource->set_value(value);
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
        /// Called for the node a *forward* extension arrives at. The derived class is handed a
        /// node id and is never told which direction asked.
        ///
        /// @param node_id Index of the node the extension arrives at.
        /// @return The bound to clamp up to, or @c nullopt.
        [[nodiscard]] virtual std::optional<V> lower_bound_at(size_t node_id) const = 0;

        /// @brief This node's upper bound, or @c nullopt for no backward clamp.
        ///
        /// Called for the node a *backward* extension arrives at.
        ///
        /// @param node_id Index of the node the extension arrives at.
        /// @return The bound to clamp down to, or @c nullopt.
        [[nodiscard]] virtual std::optional<V> upper_bound_at(size_t node_id) const = 0;

    private:
        std::optional<V> lower_;
        std::optional<V> upper_;

        /// @brief Caches both of this arc's bounds.
        ///
        /// Going forward you arrive at the arc's destination; going backward, at its origin.
        /// This is the single line where that asymmetry lives, and it is the line both of the
        /// library's historical @c extend_back defects got wrong.
        ///
        /// @param origin_id      Index of the arc's origin node.
        /// @param destination_id Index of the arc's destination node.
        void preprocess(size_t origin_id, size_t destination_id) final {
            lower_ = lower_bound_at(destination_id);
            upper_ = upper_bound_at(origin_id);
        }
};

/// @brief Every threshold resource in this library is a *translation*, so remove even the signs.
///
/// Forward adds the arc's consumption; backward subtracts it. The unsigned case saturates rather
/// than wrapping: an unsigned subtraction that underflows becomes a huge positive value, which
/// reads as a very loose bound. Saturation does not make an unsigned threshold *correct* -- that
/// is why @c kind is @c Unspecified for an unsigned @p V -- it only stops a direct call from
/// manufacturing a plausible-looking number.
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
