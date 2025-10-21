// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <memory>
#include <utility>
#include <vector>

#include "rcspp/graph/node.hpp"
#include "rcspp/graph/row.hpp"
#include "rcspp/resource/base/expander.hpp"
#include "rcspp/resource/base/resource.hpp"

namespace rcspp {

template <typename ResourceType>
// requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class Arc {
    public:
        Arc(size_t arc_id, const Node<ResourceType>& origin_node,
            const Node<ResourceType>& destination_node,
            std::unique_ptr<Expander<ResourceType>> arc_expander, double arc_cost,
            std::vector<Row> dual_rows = {})
            : id(arc_id),
              origin(origin_node),
              destination(destination_node),
              expander(std::move(arc_expander)),
              cost(arc_cost),
              dual_rows(std::move(dual_rows)) {}

        Arc(size_t arc_id, const Node<ResourceType>& origin_node,
            const Node<ResourceType>& destination_node, double arc_cost,
            std::vector<Row> dual_rows = {})
            : Arc(arc_id, origin_node, destination_node, nullptr, arc_cost, std::move(dual_rows)) {}

        Arc(size_t arc_id, const Node<ResourceType>& origin_node,
            const Node<ResourceType>& destination_node, std::vector<Row> dual_rows = {})
            : Arc(arc_id, origin_node, destination_node, 0, std::move(dual_rows)) {}

        const size_t id;

        const Node<ResourceType>& origin;

        const Node<ResourceType>& destination;

        std::unique_ptr<Expander<ResourceType>> expander;

        double cost;

        std::vector<Row> dual_rows;
};
}  // namespace rcspp
