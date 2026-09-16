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

/// @brief Declares a shape and nothing else: for functions whose backward form genuinely *is*
///        their forward one.
///
/// It exists so that "I considered the backward direction and the default is right" is
/// distinguishable from "I never considered it", which is the whole reason @c Unspecified is the
/// base class default. It deliberately does **not** finalise @c extend or @c extend_back: an
/// @c Accumulate resource, or a @c Mirror one whose arc value is genuine per-arc data, writes its
/// own @c extend and inherits @c extend_back.
///
/// @tparam Base The base to insert above -- @c ExtensionFunction<R> in every current use.
/// @tparam Kind The shape this function declares.
template <typename Base, BackwardKind Kind>
class DeclaredKindForm : public Base {
        // A class that means "no shape" must say so by NOT using a form: derive straight from
        // ExtensionFunction and let the base default stand. That is the documented escape hatch,
        // and this assertion is what keeps it honest.
        static_assert(Kind != BackwardKind::Unspecified,
                      "DeclaredKindForm is for declaring a shape; to declare none, derive from "
                      "ExtensionFunction directly and let the default stand");

    public:
        static constexpr BackwardKind kind = Kind;

        [[nodiscard]] BackwardKind backward_kind() const final { return kind; }
};

/// @brief The shape base for a container resource whose backward form swaps a *node identity*.
///
/// The ng-path defect this removes: the author is handed @c origin and @c destination and has to
/// remember which one is "the node being left" in each direction. So this form does not show
/// them. It builds one @c Side per direction and the derived class writes a single formula that
/// consumes a @c Side without knowing which it is.
///
/// It also drops the arc's extender value from @c apply's inputs entirely. For a node-identity
/// mirror the arc value *is* the wrong thing to use -- that is the finding, made structural.
///
/// **A @c Side names two nodes, not one.** A traversal has a node it *leaves* and a node it
/// *arrives at*, and a narrowing container needs both: the node left is what joins the memory, and
/// the arrival node's neighborhood is what the memory is filtered by. Carrying only the node left
/// -- which this form did until the join was found to over-reject -- means the stored memory has
/// not yet been filtered by the node it sits at, so two halves meeting there compare
/// one-step-stale sets. See @c merge_form.hpp.
///
/// @note A container resource whose arc value is genuine per-arc data is **not** this shape. It
///       is a `DeclaredKindForm<ExtensionFunction<R>, BackwardKind::Mirror>` that writes its own
///       @c extend, which is what the three container markers in this library are.
///
/// @tparam R    The container resource type.
/// @tparam Base The base to insert above -- @c ExtensionFunction<R> in every current use.
template <typename R, typename Base>
class NodeMirrorForm : public Base {
    public:
        static constexpr BackwardKind kind = BackwardKind::Mirror;

        [[nodiscard]] BackwardKind backward_kind() const final { return kind; }

        /// @brief Forward extension: leaves the arc's origin, arrives at its destination.
        ///
        /// @param resource          Current container resource of the forward label.
        /// @param extender_value    Arc's extender resource (unused; see the class note).
        /// @param extended_resource Output: receives the new container value.
        void extend(const R& resource, const R& /*extender_value*/, R* extended_resource) final {
            apply(resource, extended_resource, forward_side_);
        }

        /// @brief Backward extension: the same formula, leaving the destination and arriving at
        ///        the origin.
        ///
        /// @param resource          Current container resource of the backward label.
        /// @param extender_value    Arc's extender resource (unused; see the class note).
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

        /// @brief The formula, written once, direction-blind.
        ///
        /// @param resource          The label's current value.
        /// @param extended_resource Output: receives the new value.
        /// @param side              This traversal's node left and arrival neighborhood.
        virtual void apply(const R& resource, R* extended_resource, const Side& side) const = 0;

        /// @brief Builds the side for one traversal.
        ///
        /// Both ids are supplied because the two halves of a @c Side come from *different* nodes;
        /// a derived class that reads only @p node_left_id is the defect this signature exists to
        /// prevent.
        ///
        /// @param node_left_id    Index of the node the label leaves.
        /// @param node_arrived_id Index of the node the label arrives at.
        /// @return The singleton of the node left, and the neighborhood of the node arrived at.
        [[nodiscard]] virtual Side make_side(size_t node_left_id, size_t node_arrived_id) const = 0;

    private:
        Side forward_side_;
        Side backward_side_;

        /// @brief Caches both of this arc's sides.
        ///
        /// Going forward you leave the arc's origin and arrive at its destination; going backward,
        /// the other way round. This is the line the ng-path defect got wrong, and it is written
        /// once. Note that the two arguments are simply swapped between the directions -- if that
        /// ever stops being true, the mirror is not a mirror.
        ///
        /// @param origin_id      Index of the arc's origin node.
        /// @param destination_id Index of the arc's destination node.
        void preprocess(size_t origin_id, size_t destination_id) final {
            forward_side_ = make_side(origin_id, destination_id);
            backward_side_ = make_side(destination_id, origin_id);
        }
};

}  // namespace rcspp
