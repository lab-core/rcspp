// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <concepts>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <vector>

#include "rcspp/graph/arc.hpp"
#include "rcspp/resource/base/resource_factory.hpp"

namespace rcspp {

template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class Graph {
    public:
        Graph() = default;

        virtual Node<ResourceType>& add_node(size_t node_id, bool source = false,
                                             bool sink = false) {
            nodes_by_id_[node_id] = std::make_unique<Node<ResourceType>>(node_id);

            if (source) {
                source_node_ids_.push_back(nodes_by_id_[node_id]->id);
            }

            if (sink) {
                sink_node_ids_.push_back(nodes_by_id_[node_id]->id);
            }

            return *nodes_by_id_[node_id];
        }

        virtual Arc<ResourceType>& add_arc(Node<ResourceType>& origin_node,
                                           Node<ResourceType>& destination_node,
                                           std::optional<size_t> arc_id = std::nullopt,
                                           double cost = 0.0, std::vector<Row> dual_rows = {}) {
            if (arc_id == std::nullopt) {
                arc_id = arcs_by_id_.size();
            }

            arcs_by_id_[*arc_id] = std::make_unique<Arc<ResourceType>>(*arc_id,
                                                                       origin_node,
                                                                       destination_node,
                                                                       cost,
                                                                       dual_rows);

            auto new_arc_ptr = arcs_by_id_[*arc_id].get();

            origin_node.out_arcs.push_back(new_arc_ptr);
            destination_node.in_arcs.push_back(new_arc_ptr);

            return *new_arc_ptr;
        }

        virtual Arc<ResourceType>& add_arc(size_t origin_node_id, size_t destination_node_id,
                                           std::optional<size_t> arc_id = std::nullopt,
                                           double cost = 0.0, std::vector<Row> dual_rows = {}) {
            auto& origin_node = *nodes_by_id_.at(origin_node_id);
            auto& destination_node = *nodes_by_id_.at(destination_node_id);

            return add_arc(origin_node, destination_node, arc_id, cost, dual_rows);
        }

        [[nodiscard]] Node<ResourceType>& get_node(size_t node_id) const {
            return *nodes_by_id_.at(node_id).get();
        }

        [[nodiscard]] Arc<ResourceType>& get_arc(size_t arc_id) const {
            return *arcs_by_id_.at(arc_id).get();
        }

        [[nodiscard]] std::vector<size_t> get_node_ids() const {
            auto node_ids_ranges = std::views::keys(nodes_by_id_);

            return std::vector<size_t>{node_ids_ranges.begin(), node_ids_ranges.end()};
        }

        [[nodiscard]] std::vector<size_t> get_arc_ids() const {
            auto arc_ids_ranges = std::views::keys(arcs_by_id_);

            return std::vector<size_t>{arc_ids_ranges.begin(), arc_ids_ranges.end()};
        }

        [[nodiscard]] const std::map<size_t, std::unique_ptr<Arc<ResourceType>>>& get_arcs_by_id()
            const {
            return arcs_by_id_;
        }

        [[nodiscard]] const std::vector<size_t>& get_source_node_ids() const {
            return source_node_ids_;
        }

        [[nodiscard]] const std::vector<size_t>& get_sink_node_ids() const {
            return sink_node_ids_;
        }

        [[nodiscard]] size_t get_number_of_nodes() const { return nodes_by_id_.size(); }

        [[nodiscard]] size_t get_number_of_arcs() const { return arcs_by_id_.size(); }

        [[nodiscard]] bool is_source(size_t node_id) const {
            return std::ranges::find(source_node_ids_, node_id) != source_node_ids_.end();
        }

        [[nodiscard]] bool is_sink(size_t node_id) const {
            return std::ranges::find(sink_node_ids_, node_id) != sink_node_ids_.end();
        }

    private:
        std::map<size_t, std::unique_ptr<Arc<ResourceType>>> arcs_by_id_;
        std::map<size_t, std::unique_ptr<Node<ResourceType>>> nodes_by_id_;

        std::vector<size_t> source_node_ids_;
        std::vector<size_t> sink_node_ids_;
};
}  // namespace rcspp