// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>

#include "rcspp/graph/arc.hpp"
#include "rcspp/graph/node.hpp"
#include "rcspp/resource/base/resource.hpp"

namespace rcspp {

template <typename ResourceType>
class LabelFactory;

/// @brief Represents a label in the resource-constrained shortest-path label-setting algorithm.
///
/// A label encodes the state of a partial path: the accumulated resource consumption,
/// the current node at the end of the path, and pointers to the incoming and outgoing
/// arcs used to construct the path. Labels support dominance checking and forward
/// extension along an arc.
///
/// @tparam ResourceType The resource type used to track consumption along the path.
///         Must satisfy @c ResourceTypeConcept.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Label {
        friend class LabelFactory<ResourceType>;

    public:
        /// @brief Unique identifier for this label within its factory's allocation pool.
        size_t id;

        /// @brief Constructs a label with only an id and resource; no graph position.
        ///
        /// @param label_id   Numeric identifier assigned by the owning @c LabelFactory.
        /// @param resource   Owning pointer to the resource state for this label.
        Label(size_t label_id, std::unique_ptr<Resource<ResourceType>> resource)
            : id(label_id),
              dominated(false),
              resource_(std::move(resource)),
              end_node_(nullptr),
              in_arc_(nullptr),
              out_arc_(nullptr) {}

        /// @brief Constructs a label with full graph-position information.
        ///
        /// @param label_id   Numeric identifier assigned by the owning @c LabelFactory.
        /// @param resource   Owning pointer to the resource state for this label.
        /// @param end_node   Pointer to the node at the end of the partial path.
        /// @param in_arc     Pointer to the arc via which this label was extended forward
        ///                   (may be @c nullptr for the source label).
        /// @param out_arc    Pointer to the arc via which this label was extended backward
        ///                   (may be @c nullptr for forward labels).
        Label(size_t label_id, std::unique_ptr<Resource<ResourceType>> resource,
              const Node<ResourceType>* end_node, const Arc<ResourceType>* in_arc,
              const Arc<ResourceType>* out_arc)
            : id(label_id),
              dominated(false),
              resource_(std::move(resource)),
              end_node_(end_node),
              in_arc_(in_arc),
              out_arc_(out_arc) {}

        /// @brief Tests whether this label dominates @p rhs_label.
        ///
        /// A label @c a dominates label @c b when every resource consumed by @c a is
        /// no greater than the corresponding resource consumed by @c b (i.e., @c a is
        /// at least as good as @c b in every dimension).
        ///
        /// @param rhs_label The label to compare against.
        /// @return @c true if @c *this dominates @p rhs_label.
        [[nodiscard]] bool operator<=(const Label& rhs_label) const {
            return *resource_ <= *rhs_label.resource_;
        }

        /// @brief Extends this label along @p arc and writes the result into @p extended_label.
        ///
        /// The arc's extender is invoked to propagate the resource state, and the
        /// graph-position fields of @p extended_label are updated accordingly.
        ///
        /// @param arc            The arc along which to extend.
        /// @param extended_label Output label that will hold the extended state.
        ///                       Must be a valid, pre-allocated @c Label object.
        void extend(const Arc<ResourceType>& arc, Label* extended_label) const {
            arc.extender->extend(*resource_, extended_label->resource_.get());
            extended_label->end_node_ = arc.destination;
            extended_label->in_arc_ = &arc;
            extended_label->out_arc_ = nullptr;
        }

        /// @brief Returns the accumulated cost of the partial path represented by this label.
        ///
        /// @return The cost value stored in the underlying resource.
        [[nodiscard]] double get_cost() const { return resource_->get_cost(); }

        /// @brief Returns whether this label's resource state satisfies all feasibility
        /// constraints.
        ///
        /// @return @c true if the label is feasible.
        [[nodiscard]] bool is_feasible() const { return resource_->is_feasible(); }

        /// @brief Returns whether this label can still reach the specified destination node.
        ///
        /// @param destination_node_id  The ID of the node to test reachability for.
        /// @return @c true if the destination node is reachable from the current state.
        [[nodiscard]] bool is_reachable(size_t destination_node_id) const {
            return resource_->is_reachable(destination_node_id);
        }

        /// @brief Returns a reference to the resource state tracked by this label.
        ///
        /// @return Reference to the underlying @c Resource object.
        [[nodiscard]] Resource<ResourceType>& get_resource() const { return *resource_; }

        /// @brief Returns a pointer to the node at the end of this label's partial path.
        ///
        /// @return Pointer to the end node, or @c nullptr if not yet assigned.
        [[nodiscard]] const Node<ResourceType>* get_end_node() const { return end_node_; }

        /// @brief Returns a pointer to the arc used for the most recent forward extension.
        ///
        /// @return Pointer to the incoming arc, or @c nullptr for the source label.
        [[nodiscard]] const Arc<ResourceType>* get_in_arc() const { return in_arc_; }

        /// @brief Sets the predecessor label and increments its reference count.
        ///
        /// Establishes the backward path linkage from this label to @p predecessor.
        /// The predecessor's @c ref_count is incremented to prevent premature release.
        ///
        /// @param predecessor Pointer to the predecessor label in the path.
        void set_prev_label(Label<ResourceType>* predecessor) {
            prev_label = predecessor;
            ++predecessor->ref_count;
        }

        /// @brief Flag indicating that this label has been dominated and can be discarded.
        bool dominated;

        /// @brief Predecessor label in the path; valid as long as @c ref_count keeps it pinned.
        Label<ResourceType>* prev_label = nullptr;

        /// @brief Number of live successor labels that reference this label as their predecessor.
        ///
        /// Using 32 bits to accommodate high-out-degree nodes (e.g., dense VRP pricing graphs)
        /// without overflow, which would cause @c release_with_ref_count() to free a
        /// still-referenced predecessor.
        uint32_t ref_count = 0;

        /// @brief True when the algorithm wanted to release this label but @c ref_count was > 0.
        bool pending_release = false;

    private:
        // Resource consumed by the label.
        std::unique_ptr<Resource<ResourceType>> resource_;

        // Pointer to the node at the end of the path associated with the current label.
        const Node<ResourceType>* end_node_;

        // Pointer to the arc from which this label was forward extended.
        const Arc<ResourceType>* in_arc_;

        // Pointer to the arc from which this label was backward extended.
        const Arc<ResourceType>* out_arc_;
};
}  // namespace rcspp
