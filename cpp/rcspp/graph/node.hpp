// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/base/resource_type.hpp"

namespace rcspp {

/// @brief Forward declaration of Arc for use in Node.
///
/// @tparam ResourceType The resource type used in the graph.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Arc;

/// @brief Forward declaration of Graph for friendship in Node.
///
/// @tparam ResourceType The resource type used in the graph.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Graph;

/// @brief A vertex in an RCSPP graph, holding adjacency lists, an optional node resource, and
/// topology metadata.
///
/// Each node has a unique identifier and flags indicating whether it acts as a source
/// or sink. Incoming and outgoing arcs are stored as non-owning raw pointers.
/// An optional @c Resource can be attached to enforce node-level resource constraints.
/// The sorted position @c pos() is only valid after @c Graph::sort_nodes() has been called.
///
/// @tparam ResourceType The resource type used by arcs and labels in this graph.
///         Must satisfy @c ResourceTypeConcept.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Node {
    public:
        /// @brief Constructs a node with the given id and source/sink flags.
        ///
        /// @param node_id Unique numeric identifier for this node.
        /// @param source True if this node is a source (path starting point).
        /// @param sink True if this node is a sink (path ending point).
        explicit Node(size_t node_id, bool source, bool sink)
            : id(node_id), source(source), sink(sink) {}

        /// @brief Unique numeric identifier for this node.
        const size_t id;

        /// @brief Non-owning pointers to all arcs whose destination is this node.
        std::vector<Arc<ResourceType>*> in_arcs;

        /// @brief Non-owning pointers to all arcs whose origin is this node.
        std::vector<Arc<ResourceType>*> out_arcs;

        /// @brief Optional resource attached to this node for node-level constraints.
        ///
        /// May be null if no node resource is needed.
        std::unique_ptr<Resource<ResourceType>> resource;

        /// @brief True if this node is a source (labels may start here).
        const bool source;

        /// @brief True if this node is a sink (labels may terminate here).
        const bool sink;

        /// @brief Returns the topological position of this node in the sorted graph.
        ///
        /// @return The zero-based position index assigned by @c Graph::sort_nodes().
        /// @throws std::bad_optional_access If @c Graph::sort_nodes() has not been called yet.
        [[nodiscard]] size_t pos() const {
            try {
                return pos_.value();
            } catch (const std::bad_optional_access& e) {
                LOG_FATAL("Node::pos(): Position is not set for node ",
                          std::to_string(id),
                          ". Sort the graph with Graph::sort_nodes() to set pos.\n");
                throw e;
            }
        }

        /// @brief Returns a human-readable string representation of this node.
        ///
        /// @return A string describing the node id, source/sink flags, attached resource,
        ///         and the ids of predecessor and successor nodes.
        [[nodiscard]] std::string to_string() const {
            std::stringstream ss;
            ss << "Node(id=" << id;
            if (source) {
                ss << ", source";
            }
            if (sink) {
                ss << ", sink";
            }
            ss << ")\n";
            if (resource) {
                ss << "    resource: [" << resource->to_string() << "]\n";
            }
            ss << "    predecessors: [";
            for (const auto* arc : in_arcs) {
                ss << arc->origin->id << " ";
            }
            ss << "]\n";
            ss << "    successors: [";
            for (const auto* arc : out_arcs) {
                ss << arc->destination->id << " ";
            }
            ss << "]";
            return ss.str();
        }

    private:
        friend class Graph<ResourceType>;
        std::optional<size_t> pos_;
        size_t csr_out_start_{0};
        size_t csr_out_count_{0};
        size_t csr_in_start_{0};
        size_t csr_in_count_{0};
};

/// @brief Writes a human-readable representation of @p node to the output stream @p os.
///
/// @tparam ResourceType The resource type used by the node.
/// @param os The output stream to write to.
/// @param node The node to serialize.
/// @return The same output stream @p os, to allow chaining.
template <typename ResourceType>
std::ostream& operator<<(std::ostream& os, const Node<ResourceType>& node) {
    return os << node.to_string();
}
}  // namespace rcspp
