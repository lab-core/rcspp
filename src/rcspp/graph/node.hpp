// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <memory>
#include <string>
#include <vector>

#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/base/resource_base.hpp"

namespace rcspp {

template <typename ResourceType>
class Arc;

template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class Graph;

template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class Node {
    public:
        explicit Node(size_t node_id, bool source, bool sink)
            : id(node_id), source(source), sink(sink) {}

        const size_t id;

        std::vector<Arc<ResourceType>*> in_arcs;
        std::vector<Arc<ResourceType>*> out_arcs;

        std::unique_ptr<Resource<ResourceType>> resource;

        const bool source;
        const bool sink;

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
};
template <typename ResourceType>
std::ostream& operator<<(std::ostream& os, const Node<ResourceType>& node) {
    return os << node.to_string();
}
}  // namespace rcspp
