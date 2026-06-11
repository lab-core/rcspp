// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/graph/node.hpp"
#include "rcspp/graph/row.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/base/resource_type.hpp"

namespace rcspp {

/// @brief Directed arc in an RCSPP graph connecting two nodes with a cost and optional resource
/// extender.
///
/// Each arc carries a unique identifier, pointers to its origin and destination nodes,
/// an optional resource @c Extender that propagates label state along the arc,
/// a traversal cost, and a list of LP master-problem row contributions.
///
/// @tparam ResourceType The resource type used by nodes and labels in this graph.
///         Must satisfy @c ResourceTypeConcept.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Arc {
    public:
        /// @brief Constructs an arc with a full set of attributes.
        ///
        /// @param arc_id Unique numeric identifier for this arc.
        /// @param origin_node Pointer to the tail (origin) node. Must not be null.
        /// @param destination_node Pointer to the head (destination) node. Must not be null.
        /// @param arc_extender Owning pointer to the resource extender applied when traversing this
        /// arc.
        /// @param arc_cost Traversal cost associated with this arc.
        /// @param rows LP master-problem row contributions for this arc.
        Arc(size_t arc_id, Node<ResourceType>* origin_node, Node<ResourceType>* destination_node,
            std::unique_ptr<Extender<ResourceType>> arc_extender, double arc_cost,
            std::vector<Row> rows = {})
            : id(arc_id),
              origin(origin_node),
              destination(destination_node),
              extender(std::move(arc_extender)),
              cost(arc_cost),
              rows(std::move(rows)) {}

        /// @brief Constructs an arc without a resource extender.
        ///
        /// @param arc_id Unique numeric identifier for this arc.
        /// @param origin_node Pointer to the tail (origin) node. Must not be null.
        /// @param destination_node Pointer to the head (destination) node. Must not be null.
        /// @param arc_cost Traversal cost associated with this arc.
        /// @param rows LP master-problem row contributions for this arc.
        Arc(size_t arc_id, Node<ResourceType>* origin_node, Node<ResourceType>* destination_node,
            double arc_cost, std::vector<Row> rows = {})
            : Arc(arc_id, origin_node, destination_node, nullptr, arc_cost, std::move(rows)) {}

        /// @brief Constructs a zero-cost arc without a resource extender.
        ///
        /// @param arc_id Unique numeric identifier for this arc.
        /// @param origin_node Pointer to the tail (origin) node. Must not be null.
        /// @param destination_node Pointer to the head (destination) node. Must not be null.
        /// @param rows LP master-problem row contributions for this arc.
        Arc(size_t arc_id, Node<ResourceType>* origin_node, Node<ResourceType>* destination_node,
            std::vector<Row> rows = {})
            : Arc(arc_id, origin_node, destination_node, 0, std::move(rows)) {}

        /// @brief Unique numeric identifier for this arc.
        const size_t id;

        /// @brief Pointer to the tail (origin) node of this arc.
        Node<ResourceType>* const origin;

        /// @brief Pointer to the head (destination) node of this arc.
        Node<ResourceType>* const destination;

        /// @brief Optional resource extender applied when a label traverses this arc.
        ///
        /// May be null if no resource extension is needed.
        std::unique_ptr<Extender<ResourceType>> extender;

        /// @brief Traversal cost of this arc.
        double cost;

        /// @brief LP master-problem row contributions associated with this arc.
        std::vector<Row> rows;

        /// @brief Returns a human-readable string representation of this arc.
        ///
        /// @return A string describing the arc id, origin, destination, cost, and extender (if
        /// present).
        [[nodiscard]] std::string to_string() const {
            std::stringstream ss;
            ss << "Arc(id=" << id << ", origin=" << origin->id
               << ", destination=" << destination->id << ", cost=" << cost;
            if (extender) {
                ss << ", extender=[" << extender->to_string() << "]";
            }
            ss << ")\n";
            return ss.str();
        }
};

/// @brief Writes a human-readable representation of @p arc to the output stream @p os.
///
/// @tparam ResourceType The resource type used by the arc.
/// @param os The output stream to write to.
/// @param arc The arc to serialize.
/// @return The same output stream @p os, to allow chaining.
template <typename ResourceType>
std::ostream& operator<<(std::ostream& os, const Arc<ResourceType>& arc) {
    return os << arc.to_string();
}
}  // namespace rcspp
