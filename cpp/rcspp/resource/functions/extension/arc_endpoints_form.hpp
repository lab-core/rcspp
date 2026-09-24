// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>

#include "rcspp/resource/functions/extension/backward_form.hpp"

namespace rcspp {

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
///       `BackwardForm<ExtensionFunction<R>, BackwardKind::ArcValue>` instead. Only this form
///       gives a memory that excludes its own node in both directions.
///
/// @tparam R    The container resource type.
/// @tparam Base The base to insert above, normally @c ExtensionFunction<R>.
template <typename R, typename Base>
class ArcEndpointsForm : public BackwardForm<Base, BackwardKind::ArcEndpoints> {
    public:
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
