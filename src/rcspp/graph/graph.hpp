// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>  // NOLINT(build/include_order)
#include <functional>
#include <memory>
#include <optional>
#include <ranges>  // NOLINT(build/include_order)
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "rcspp/graph/arc.hpp"
#include "rcspp/resource/base/resource_factory.hpp"

namespace rcspp {

template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Graph {
    public:
        Graph() = default;

        // copy constructor and assignment operator are deleted
        Graph(const Graph& graph) = delete;
        Graph& operator=(const Graph& graph) = delete;
        Graph(Graph&&) = delete;
        Graph& operator=(Graph&&) = delete;

        [[nodiscard]] std::unique_ptr<Graph<ResourceType>> clone(
            bool clone_removed_arcs = false) const {
            auto new_graph = std::make_unique<Graph<ResourceType>>();

            // copy nodes
            for (const auto& [node_id, node_ptr] : nodes_by_id_) {
                auto& node = new_graph->add_node(node_id, node_ptr->source, node_ptr->sink);
                node.resource =
                    node_ptr->resource ? std::move(node_ptr->resource->clone()) : nullptr;
            }

            // copy sorted nodes
            for (const auto* node_ptr : sorted_nodes_) {
                auto* node = new_graph->get_node(node_ptr->id);
                node->pos_ = node_ptr->pos_;
                new_graph->sorted_nodes_.push_back(node);
            }

            // copy arcs
            for (const auto& [arc_id, arc_ptr] : arcs_by_id_) {
                auto& arc = new_graph->add_arc(arc_ptr->origin->id,
                                               arc_ptr->destination->id,
                                               arc_ptr->cost,
                                               arc_ptr->dual_rows,
                                               arc_id);
                arc.extender =
                    arc_ptr->extender ? std::move(arc_ptr->extender->clone(arc)) : nullptr;
            }

            if (clone_removed_arcs) {
                // copy removed arcs
                for (const auto& [arc_id, arc_ptr] : removed_arcs_by_id_) {
                    auto& arc = new_graph->add_arc(arc_ptr->origin->id,
                                                   arc_ptr->destination->id,
                                                   arc_ptr->cost,
                                                   arc_ptr->dual_rows,
                                                   arc_id);
                    new_graph->remove_arc(arc_id);
                    arc.extender =
                        arc_ptr->extender ? std::move(arc_ptr->extender->clone(arc)) : nullptr;
                }
            }

            return new_graph;
        }

        virtual Node<ResourceType>& add_node(size_t node_id, bool source = false,
                                             bool sink = false) {
            nodes_by_id_[node_id] = std::make_unique<Node<ResourceType>>(node_id, source, sink);
            modified_ = true;
            csr_valid_ = false;

            if (source) {
                source_node_ids_.push_back(nodes_by_id_[node_id]->id);
            }

            if (sink) {
                sink_node_ids_.push_back(nodes_by_id_[node_id]->id);
            }

            return *nodes_by_id_[node_id];
        }

        virtual Arc<ResourceType>& add_arc(Node<ResourceType>* origin_node,
                                           Node<ResourceType>* destination_node, double cost = 0.0,
                                           std::vector<Row> dual_rows = {},
                                           std::optional<size_t> arc_id = std::nullopt) {
            if (arc_id == std::nullopt) {
                arc_id = next_arc_id_;
            }
            next_arc_id_ = std::max(next_arc_id_, *arc_id + 1);

            auto& new_arc = arcs_by_id_[*arc_id] =
                std::make_unique<Arc<ResourceType>>(*arc_id,
                                                    origin_node,
                                                    destination_node,
                                                    cost,
                                                    dual_rows);
            modified_ = true;
            csr_valid_ = false;

            origin_node->out_arcs.push_back(new_arc.get());
            destination_node->in_arcs.push_back(new_arc.get());

            return *new_arc.get();
        }

        virtual Arc<ResourceType>& add_arc(size_t origin_node_id, size_t destination_node_id,
                                           double cost = 0.0, std::vector<Row> dual_rows = {},
                                           std::optional<size_t> arc_id = std::nullopt) {
            auto& origin_node = nodes_by_id_.at(origin_node_id);
            auto& destination_node = nodes_by_id_.at(destination_node_id);

            return add_arc(origin_node.get(), destination_node.get(), cost, dual_rows, arc_id);
        }

        virtual bool remove_arc(size_t arc_id) {
            auto it = arcs_by_id_.find(arc_id);
            if (it == arcs_by_id_.end()) {
                return false;
            }
            remove_arc(it);
            return true;
        }

        virtual bool remove_arc(const Arc<ResourceType>& arc) { return remove_arc(arc.id); }

        template <typename C>
        std::vector<size_t> remove_arcs_if(C check) {
            std::vector<size_t> deleted_arc_ids;
            for (auto it = arcs_by_id_.begin(); it != arcs_by_id_.end();) {
                // check if we should remove the arc
                if (check(*it->second)) {
                    deleted_arc_ids.push_back(it->first);
                    it = remove_arc(it);
                } else {
                    ++it;
                }
            }
            return deleted_arc_ids;
        }

        virtual bool restore_arc(size_t arc_id) {
            auto it = removed_arcs_by_id_.find(arc_id);
            if (it == removed_arcs_by_id_.end()) {
                return false;
            }
            restore_arc(it);
            return true;
        }

        virtual bool restore_arc(const Arc<ResourceType>& arc) { return restore_arc(arc.id); }

        template <typename C>
        std::vector<size_t> restore_arcs_if(C check) {
            std::vector<size_t> restored_arc_ids;
            for (auto it = removed_arcs_by_id_.begin(); it != removed_arcs_by_id_.end();) {
                // check if we should remove the arc
                if (check(*it->second)) {
                    restored_arc_ids.push_back(it->first);
                    it = restore_arc(it);
                } else {
                    ++it;
                }
            }
            return restored_arc_ids;
        }

        [[nodiscard]] Node<ResourceType>* get_node(size_t node_id) const {
            auto it = nodes_by_id_.find(node_id);
            if (it == nodes_by_id_.end()) {
                return nullptr;
            }
            return it->second.get();
        }

        [[nodiscard]] Arc<ResourceType>* get_arc(size_t arc_id) const {
            auto it = arcs_by_id_.find(arc_id);
            if (it == arcs_by_id_.end()) {
                return nullptr;
            }
            return it->second.get();
        }

        [[nodiscard]] std::vector<Arc<ResourceType>*> get_arcs(size_t ori_id,
                                                               size_t dest_id) const {
            std::vector<Arc<ResourceType>*> arcs;
            auto* ori = get_node(ori_id);
            if (ori != nullptr) {
                for (auto* arc : ori->out_arcs) {
                    if (arc->destination->id == dest_id) {
                        arcs.push_back(arc);
                    }
                }
            }
            return arcs;
        }

        [[nodiscard]] std::vector<size_t> get_node_ids() const {
            std::vector<size_t> ids;
            ids.reserve(nodes_by_id_.size());
            for (const auto& [k, _] : nodes_by_id_) {
                ids.push_back(k);
            }
            std::sort(ids.begin(), ids.end());
            return ids;
        }

        [[nodiscard]] size_t get_nodes_size() const { return nodes_by_id_.size(); }

        [[nodiscard]] std::vector<size_t> get_arc_ids() const {
            std::vector<size_t> ids;
            ids.reserve(arcs_by_id_.size());
            for (const auto& [k, _] : arcs_by_id_) {
                ids.push_back(k);
            }
            std::sort(ids.begin(), ids.end());
            return ids;
        }

        [[nodiscard]] size_t get_arcs_size() const { return nodes_by_id_.size(); }

        [[nodiscard]] std::vector<size_t> get_removed_arc_ids() const {
            std::vector<size_t> ids;
            ids.reserve(removed_arcs_by_id_.size());
            for (const auto& [k, _] : removed_arcs_by_id_) {
                ids.push_back(k);
            }
            std::sort(ids.begin(), ids.end());
            return ids;
        }

        [[nodiscard]] Arc<ResourceType>* get_removed_arc(size_t arc_id) const {
            auto it = removed_arcs_by_id_.find(arc_id);
            return it != removed_arcs_by_id_.end() ? it->second.get() : nullptr;
        }

        [[nodiscard]] const std::unordered_map<size_t, std::unique_ptr<Arc<ResourceType>>>&
        get_arcs_by_id() const {
            return arcs_by_id_;
        }

        [[nodiscard]] const std::vector<Node<ResourceType>*>& get_sorted_nodes() const {
            return sorted_nodes_;
        }

        [[nodiscard]] const std::vector<size_t>& get_source_node_ids() const {
            return source_node_ids_;
        }

        [[nodiscard]] const std::vector<size_t>& get_sink_node_ids() const {
            return sink_node_ids_;
        }

        [[nodiscard]] size_t get_number_of_nodes() const { return nodes_by_id_.size(); }

        [[nodiscard]] size_t get_number_of_arcs() const { return arcs_by_id_.size(); }

        // Pre-allocate hash-map buckets to avoid rehashing during bulk inserts.
        void reserve(size_t n_nodes, size_t n_arcs) {
            nodes_by_id_.reserve(n_nodes);
            arcs_by_id_.reserve(n_arcs);
        }

        [[nodiscard]] bool is_source(size_t node_id) const {
            return std::ranges::find(source_node_ids_, node_id) != source_node_ids_.end();
        }

        [[nodiscard]] bool is_sink(size_t node_id) const {
            return std::ranges::find(sink_node_ids_, node_id) != sink_node_ids_.end();
        }

        void sort_nodes() {
            sort_nodes([](const Node<ResourceType>* n1, const Node<ResourceType>* n2) {
                return n1->id < n2->id;
            });
        }

        template <class Compare>
        void sort_nodes(Compare comp) {
            // populate the vector
            sorted_nodes_.clear();
            sorted_nodes_.reserve(nodes_by_id_.size());
            for (auto& [node_id, node_ptr] : nodes_by_id_) {
                sorted_nodes_.push_back(node_ptr.get());
            }

            // sort
            std::stable_sort(sorted_nodes_.begin(), sorted_nodes_.end(), comp);

            // fix position
            size_t i = 0;
            for (const auto& node_ptr : sorted_nodes_) {
                node_ptr->pos_ = i++;
            }
        }

        // Build the CSR (Compressed Sparse Row) layout from current node->out_arcs/in_arcs.
        // Call this after sorting and after any arc additions/removals to keep the hot-path
        // iteration arrays cache-coherent across sorted nodes.
        void build_csr() {
            if (csr_valid_) {
                return;
            }

            // sort nodes by id if not already sorted
            if (!are_nodes_sorted()) {
                sort_nodes();
            }

            csr_out_arcs_.clear();
            csr_in_arcs_.clear();

            size_t total_out = 0;
            size_t total_in = 0;
            for (const auto* node : sorted_nodes_) {
                total_out += node->out_arcs.size();
                total_in += node->in_arcs.size();
            }
            csr_out_arcs_.reserve(total_out);
            csr_in_arcs_.reserve(total_in);

            for (auto* node : sorted_nodes_) {
                node->csr_out_start_ = csr_out_arcs_.size();
                for (auto* arc : node->out_arcs) {
                    csr_out_arcs_.push_back(arc);
                }
                node->csr_out_count_ = csr_out_arcs_.size() - node->csr_out_start_;

                node->csr_in_start_ = csr_in_arcs_.size();
                for (auto* arc : node->in_arcs) {
                    csr_in_arcs_.push_back(arc);
                }
                node->csr_in_count_ = csr_in_arcs_.size() - node->csr_in_start_;
            }

            csr_valid_ = true;
        }

        // Zero-copy span over the outgoing arcs for a node in the CSR layout.
        // Requires csr_valid_ == true (guaranteed after build_csr()).
        [[nodiscard]] std::span<Arc<ResourceType>*> get_out_arcs(
            const Node<ResourceType>* node) const {
            return {csr_out_arcs_.data() + node->csr_out_start_, node->csr_out_count_};
        }

        // Zero-copy span over the incoming arcs for a node in the CSR layout.
        [[nodiscard]] std::span<Arc<ResourceType>*> get_in_arcs(
            const Node<ResourceType>* node) const {
            return {csr_in_arcs_.data() + node->csr_in_start_, node->csr_in_count_};
        }

        [[nodiscard]] bool are_nodes_sorted() const {
            if (sorted_nodes_.empty()) {
                return false;
            }
            for (size_t i = 0; i < sorted_nodes_.size(); i++) {
                if (sorted_nodes_[i]->pos() != i) {
                    LOG_WARN(
                        "Nodes are not correctly sorted in the graph. It will be overridden.\n");
                    return false;
                }
            }
            return true;
        }

        void track_modifications() { modified_ = false; }

        [[nodiscard]] bool is_modified() const { return modified_; }

        [[nodiscard]] std::string to_string(bool print_arcs = false) const {
            std::stringstream ss;
            ss << "Graph with " << get_number_of_nodes() << " nodes and " << get_number_of_arcs()
               << " arcs.\n";
            for (size_t id : get_node_ids()) {
                ss << *nodes_by_id_.at(id) << "\n";
            }
            if (print_arcs) {
                for (size_t id : get_arc_ids()) {
                    ss << *arcs_by_id_.at(id);
                }
            }
            return ss.str();
        }

    private:
        using ArcMap = std::unordered_map<size_t, std::unique_ptr<Arc<ResourceType>>>;
        using NodeMap = std::unordered_map<size_t, std::unique_ptr<Node<ResourceType>>>;

        ArcMap arcs_by_id_;
        NodeMap nodes_by_id_;
        std::vector<Node<ResourceType>*> sorted_nodes_;
        bool modified_ = false;
        size_t next_arc_id_ = 0;

        ArcMap removed_arcs_by_id_;

        std::vector<size_t> source_node_ids_;
        std::vector<size_t> sink_node_ids_;

        // CSR (Compressed Sparse Row) arc arrays — rebuilt by build_csr() / sort_nodes().
        // Declared mutable so const accessor methods (get_out_arcs / get_in_arcs) can return
        // non-const spans without requiring a const_cast at every call site.
        mutable std::vector<Arc<ResourceType>*> csr_out_arcs_;
        mutable std::vector<Arc<ResourceType>*> csr_in_arcs_;
        mutable bool csr_valid_ = false;

        virtual typename ArcMap::iterator remove_arc(typename ArcMap::iterator it) {
            size_t arc_id = it->first;
            Arc<ResourceType>& arc = *it->second;

            // remove arc from destination node's in_arcs
            auto& in_arcs = arc.destination->in_arcs;
            in_arcs.erase(
                std::remove_if(in_arcs.begin(),
                               in_arcs.end(),
                               [arc_id](Arc<ResourceType>* a) { return a->id == arc_id; }),
                in_arcs.end());

            // remove arc from origin node's out_arcs
            auto& out_arcs = arc.origin->out_arcs;
            out_arcs.erase(
                std::remove_if(out_arcs.begin(),
                               out_arcs.end(),
                               [arc_id](Arc<ResourceType>* a) { return a->id == arc_id; }),
                out_arcs.end());

            // move deleted arc
            removed_arcs_by_id_.emplace(arc_id, std::move(it->second));
            modified_ = true;
            csr_valid_ = false;
            // delete from arcs map
            return arcs_by_id_.erase(it);
        }

        virtual typename ArcMap::iterator restore_arc(const typename ArcMap::iterator& it) {
            Arc<ResourceType>* arc = it->second.get();
            // add arc to destination node's in_arcs
            arc->destination->in_arcs.push_back(arc);
            // add arc to origin node's out_arcs
            arc->origin->out_arcs.push_back(arc);
            // move restored arc
            arcs_by_id_.emplace(it->first, std::move(it->second));
            modified_ = true;
            csr_valid_ = false;
            // delete from deleted arcs map
            return removed_arcs_by_id_.erase(it);
        }
};

template <typename ResourceType>
std::ostream& operator<<(std::ostream& os, const Graph<ResourceType>& graph) {
    return os << graph.to_string();
}
}  // namespace rcspp
