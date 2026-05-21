// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/graph/node.hpp"
#include "rcspp/graph/row.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/base/resource_type.hpp"

namespace rcspp {

template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Arc {
    public:
        Arc(size_t arc_id, Node<ResourceType>* origin_node, Node<ResourceType>* destination_node,
            std::unique_ptr<Extender<ResourceType>> arc_extender, double arc_cost,
            std::vector<Row> dual_rows = {})
            : id(arc_id),
              origin(origin_node),
              destination(destination_node),
              extender(std::move(arc_extender)),
              cost(arc_cost),
              dual_rows(std::move(dual_rows)) {}

        Arc(size_t arc_id, Node<ResourceType>* origin_node, Node<ResourceType>* destination_node,
            double arc_cost, std::vector<Row> dual_rows = {})
            : Arc(arc_id, origin_node, destination_node, nullptr, arc_cost, std::move(dual_rows)) {}

        Arc(size_t arc_id, Node<ResourceType>* origin_node, Node<ResourceType>* destination_node,
            std::vector<Row> dual_rows = {})
            : Arc(arc_id, origin_node, destination_node, 0, std::move(dual_rows)) {}

        const size_t id;

        Node<ResourceType>* const origin;

        Node<ResourceType>* const destination;

        std::unique_ptr<Extender<ResourceType>> extender;

        double cost;

        std::vector<Row> dual_rows;

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
template <typename ResourceType>
std::ostream& operator<<(std::ostream& os, const Arc<ResourceType>& arc) {
    return os << arc.to_string();
}
}  // namespace rcspp
