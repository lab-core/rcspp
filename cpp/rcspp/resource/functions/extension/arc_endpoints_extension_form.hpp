// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>

#include "rcspp/resource/functions/extension/directional_extension_form.hpp"

namespace rcspp {

/// @brief Base for a set resource whose arc contribution is one of the arc's nodes, such as the
///        ng-route memory (the set of visited nodes a label remembers).
///
/// The derived class writes one extension formula (@c apply) that does not depend on the
/// direction. The formula reads a @c Side: the node the label leaves, to add to the set, and the
/// neighborhood of the node it arrives at, to intersect the set with. This form builds the
/// forward side (leave the origin, arrive at the destination) and the backward side (the
/// reverse) from the arc's endpoints, so the swap is written once. The arc's value is not used.
///
/// Why it exists: a memory adds "the node the label leaves". Going backward over arc (u, v), the
/// label leaves v, not u. A memory filled from the arc's value (`{u}`) would add u in both
/// directions, so a backward label at u would hold u itself and be rejected at once. This form
/// reads the node from the arc's ends instead, and swaps them for the backward direction.
/// @c NgPathExtensionFunction is built on it.
///
/// Because the set is filtered on arrival, a forward and a backward label meeting at a node both
/// hold sets filtered by that node, and the join can compare them directly.
///
/// @note Use `DirectionalExtensionForm<ExtensionFunction<ResourceType>,
/// DirectionalResourceKind::ArcValue>` instead when the arc's
///       value is real per-arc data. Only this form keeps the label's own node out of its set in
///       both directions.
///
/// @tparam ResourceType    The container resource type.
/// @tparam Base The base to insert above, normally @c ExtensionFunction<ResourceType>.
template <typename ResourceType, typename Base>
class ArcEndpointsExtensionForm
    : public DirectionalExtensionForm<Base, DirectionalResourceKind::ArcEndpoints> {
    public:
        /// @brief Forward extension: leaves the arc's origin, arrives at its destination.
        ///
        /// @param resource          Current container resource of the forward label.
        /// @param extender_value    Arc's extender resource: unused, except by
        ///                          @ref check_arc_value in a debug build.
        /// @param extended_resource Output: receives the new container value.
        void extend(const ResourceType& resource,
                    [[maybe_unused]] const ResourceType& extender_value,
                    ResourceType* extended_resource) final {
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
        void extend_back(const ResourceType& resource, const ResourceType& /*extender_value*/,
                         ResourceType* extended_resource) final {
            apply(resource, extended_resource, backward_side_);
        }

    protected:
        /// @brief One traversal of this arc, oriented: what is left, and what is arrived at.
        ///
        /// Both sets depend on one node only, so the derived class keeps one copy per node and
        /// every arc points at it: a per-arc copy would cost memory in the arcs times the
        /// neighborhood size (times the node count for an elementary path). The pointed-to sets
        /// must outlive this function and its clones, and never change.
        struct Side {
                /// the set {node being left}, added to the label's set
                const ResourceType* node_left = nullptr;
                /// the neighborhood of the node arrived at, which the label's set is
                /// intersected with
                const ResourceType* arrival_neighborhood = nullptr;
        };

        /// @brief Debug-build hook to inspect the arc's value, which the form never reads.
        ///
        /// Called on every forward extension when @c NDEBUG is not defined. The default accepts
        /// anything.
        ///
        /// @param extender_value The arc's value.
        /// @param side           This traversal's node left and arrival neighborhood.
        virtual void check_arc_value(const ResourceType& /*extender_value*/,
                                     const Side& /*side*/) const {}

        /// @brief The direction-blind extension formula.
        ///
        /// @param resource          The label's current value.
        /// @param extended_resource Output: receives the new value.
        /// @param side              This traversal's node left and arrival neighborhood.
        virtual void apply(const ResourceType& resource, ResourceType* extended_resource,
                           const Side& side) const = 0;

        /// @brief Builds the side for one traversal.
        ///
        /// @param node_left_id    Index of the node the label leaves.
        /// @param node_arrived_id Index of the node the label arrives at.
        /// @return Pointers to the singleton of the node left and to the neighborhood of the node
        ///         arrived at, each shared by every arc that reads that node.
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
