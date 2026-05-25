// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>  // NOLINT(build/include_order)
#include <functional>
#include <memory>
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
            clone_topology_into(*new_graph, /*include_rows=*/true, clone_removed_arcs);
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
                                           std::vector<Row> rows = {}) {
            return add_arc_at(origin_node, destination_node, cost, rows, next_arc_id_);
        }

        virtual Arc<ResourceType>& add_arc(size_t origin_node_id, size_t destination_node_id,
                                           double cost = 0.0, std::vector<Row> rows = {}) {
            auto& origin_node = nodes_by_id_.at(origin_node_id);
            auto& destination_node = nodes_by_id_.at(destination_node_id);

            return add_arc(origin_node.get(), destination_node.get(), cost, rows);
        }

        virtual bool remove_arc(size_t arc_id) {
            if (arc_id >= arcs_.size() || !arcs_[arc_id]) {
                return false;
            }

            Arc<ResourceType>& arc = *arcs_[arc_id];

            auto& in_arcs = arc.destination->in_arcs;
            in_arcs.erase(
                std::remove_if(in_arcs.begin(),
                               in_arcs.end(),
                               [arc_id](Arc<ResourceType>* a) { return a->id == arc_id; }),
                in_arcs.end());

            auto& out_arcs = arc.origin->out_arcs;
            out_arcs.erase(
                std::remove_if(out_arcs.begin(),
                               out_arcs.end(),
                               [arc_id](Arc<ResourceType>* a) { return a->id == arc_id; }),
                out_arcs.end());

            removed_arcs_by_id_.emplace(arc_id, std::move(arcs_[arc_id]));
            --active_arc_count_;
            modified_ = true;
            csr_valid_ = false;
            return true;
        }

        virtual bool remove_arc(const Arc<ResourceType>& arc) { return remove_arc(arc.id); }

        // Remove a batch of arcs by id. Returns the ids that were actually removed.
        std::vector<size_t> remove_arcs(const std::vector<size_t>& arc_ids) {
            std::vector<size_t> removed;
            removed.reserve(arc_ids.size());
            for (size_t id : arc_ids) {
                if (remove_arc(id)) {
                    removed.push_back(id);
                }
            }
            return removed;
        }

        // Force an arc: remove all other out-arcs from its origin and all other
        // in-arcs to its destination, keeping only this arc active on both ends.
        // Returns the ids of the arcs that were removed.
        std::vector<size_t> force_arc(size_t arc_id) {
            if (arc_id >= arcs_.size() || !arcs_[arc_id]) {
                return {};
            }
            Arc<ResourceType>& arc = *arcs_[arc_id];

            std::vector<size_t> to_remove;
            for (auto* a : arc.origin->out_arcs) {
                if (a->id != arc_id) {
                    to_remove.push_back(a->id);
                }
            }
            for (auto* a : arc.destination->in_arcs) {
                if (a->id != arc_id) {
                    to_remove.push_back(a->id);
                }
            }

            // deduplicate (an arc from origin→destination would appear in both lists)
            std::ranges::sort(to_remove);
            to_remove.erase(std::ranges::unique(to_remove).begin(), to_remove.end());

            for (size_t id : to_remove) {
                remove_arc(id);
            }
            return to_remove;
        }

        std::vector<size_t> force_arc(const Arc<ResourceType>& arc) { return force_arc(arc.id); }

        template <typename C>
        std::vector<size_t> remove_arcs_if(C check) {
            std::vector<size_t> to_remove;
            for_each_arc([&](const auto& arc) {
                if (check(arc)) {
                    to_remove.push_back(arc.id);
                }
            });
            for (size_t id : to_remove) {
                remove_arc(id);
            }
            return to_remove;
        }

        virtual bool restore_arc(size_t arc_id) {
            auto it = removed_arcs_by_id_.find(arc_id);
            if (it == removed_arcs_by_id_.end()) {
                return false;
            }

            Arc<ResourceType>* arc = it->second.get();
            arc->destination->in_arcs.push_back(arc);
            arc->origin->out_arcs.push_back(arc);

            if (arc_id >= arcs_.size()) {
                arcs_.resize(arc_id + 1);
            }
            arcs_[arc_id] = std::move(it->second);
            removed_arcs_by_id_.erase(it);
            ++active_arc_count_;
            modified_ = true;
            csr_valid_ = false;
            return true;
        }

        virtual bool restore_arc(const Arc<ResourceType>& arc) { return restore_arc(arc.id); }

        // Restore a batch of arcs by id. Returns the ids that were actually restored.
        std::vector<size_t> restore_arcs(const std::vector<size_t>& arc_ids) {
            std::vector<size_t> restored;
            restored.reserve(arc_ids.size());
            for (size_t id : arc_ids) {
                if (restore_arc(id)) {
                    restored.push_back(id);
                }
            }
            return restored;
        }

        template <typename C>
        std::vector<size_t> restore_arcs_if(C check) {
            std::vector<size_t> restored_arc_ids;
            for (auto it = removed_arcs_by_id_.begin(); it != removed_arcs_by_id_.end();) {
                if (check(*it->second)) {
                    restored_arc_ids.push_back(it->first);
                    it = restore_arc_from_map(it);
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
            if (arc_id >= arcs_.size()) {
                return nullptr;
            }
            return arcs_[arc_id].get();
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

        /// @brief Iterates over all active arcs without allocating, passing a mutable Arc&.
        template <typename F>
        void for_each_arc(F&& fn) {
            for (auto& arc : arcs_) {
                if (arc) {
                    fn(*arc);
                }
            }
        }

        /// @brief Iterates over all active arcs without allocating, passing a const Arc&.
        template <typename F>
        void for_each_arc(F&& fn) const {
            for (const auto& arc : arcs_) {
                if (arc) {
                    fn(*arc);
                }
            }
        }

        [[nodiscard]] size_t get_arcs_size() const { return active_arc_count_; }

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

        [[nodiscard]] size_t get_number_of_arcs() const { return active_arc_count_; }

        /// @brief Append *rows* to the rows of arc *arc_id*.
        /// @return True if the arc was found and updated, false if *arc_id* is invalid.
        bool add_rows_to_arc(size_t arc_id, const std::vector<Row>& rows) {
            if (arc_id >= arcs_.size() || !arcs_[arc_id]) {
                return false;
            }
            auto& dr = arcs_[arc_id]->rows;
            dr.insert(dr.end(), rows.begin(), rows.end());
            return true;
        }

        /// @brief Return the next arc ID that will be assigned by add_arc().
        [[nodiscard]] size_t next_arc_id() const { return next_arc_id_; }

        // Pre-allocate storage to avoid reallocation during bulk inserts.
        void reserve(size_t n_nodes, size_t n_arcs) {
            nodes_by_id_.reserve(n_nodes);
            arcs_.reserve(n_arcs);
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
            sorted_nodes_.clear();
            sorted_nodes_.reserve(nodes_by_id_.size());
            for (auto& [node_id, node_ptr] : nodes_by_id_) {
                sorted_nodes_.push_back(node_ptr.get());
            }
            std::stable_sort(sorted_nodes_.begin(), sorted_nodes_.end(), comp);
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

            csr_out_arcs_.clear();
            csr_in_arcs_.clear();

            size_t total_out = 0;
            size_t total_in = 0;
            for (const auto& [id, node] : nodes_by_id_) {
                total_out += node->out_arcs.size();
                total_in += node->in_arcs.size();
            }
            csr_out_arcs_.reserve(total_out);
            csr_in_arcs_.reserve(total_in);

            for (auto& [id, node] : nodes_by_id_) {
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
                for_each_arc([&](const auto& arc) { ss << arc; });
            }
            return ss.str();
        }

    protected:
        /// @brief Copy nodes, sorted-node order, and arcs from *this* into *target*.
        ///
        /// Called by clone() and by ResourceGraph::clone(). Being a Graph<ResourceType>
        /// member it has access to all private fields of both *this* and *target*
        /// (nodes_by_id_, sorted_nodes_, removed_arcs_by_id_, add_arc_at).
        /// Node creation goes through target.add_node() (virtual) so that a
        /// ResourceGraph target correctly initialises node resources from its factory;
        /// the resource is then overridden with a clone of the original's resource.
        ///
        /// @param target            Graph to fill; must be empty on entry.
        /// @param include_rows      When false, arc rows are left empty.
        /// @param clone_removed_arcs Also copy removed arcs (re-removed in target).
        void clone_topology_into(Graph<ResourceType>& target, bool include_rows,
                                 bool clone_removed_arcs) const {
            target.reserve(get_nodes_size(), get_arcs_size());

            for (const auto& [node_id, node_ptr] : nodes_by_id_) {
                auto& node = target.add_node(node_id, node_ptr->source, node_ptr->sink);
                node.resource = node_ptr->resource ? node_ptr->resource->clone() : nullptr;
            }

            for (const auto* node_ptr : sorted_nodes_) {
                auto* node = target.get_node(node_ptr->id);
                node->pos_ = node_ptr->pos_;
                target.sorted_nodes_.push_back(node);
            }

            for_each_arc([&](const auto& arc_ref) {
                auto* origin = target.get_node(arc_ref.origin->id);
                auto* dest = target.get_node(arc_ref.destination->id);
                auto& arc = target.add_arc_at(origin,
                                              dest,
                                              arc_ref.cost,
                                              include_rows ? arc_ref.rows : std::vector<Row>{},
                                              arc_ref.id);
                arc.extender = arc_ref.extender ? std::move(arc_ref.extender->clone(arc)) : nullptr;
            });

            if (clone_removed_arcs) {
                for (const auto& [arc_id, arc_ptr] : removed_arcs_by_id_) {
                    auto* origin = target.get_node(arc_ptr->origin->id);
                    auto* dest = target.get_node(arc_ptr->destination->id);
                    auto& arc = target.add_arc_at(origin,
                                                  dest,
                                                  arc_ptr->cost,
                                                  include_rows ? arc_ptr->rows : std::vector<Row>{},
                                                  arc_id);
                    arc.extender =
                        arc_ptr->extender ? std::move(arc_ptr->extender->clone(arc)) : nullptr;
                    target.remove_arc(arc_id);
                }
            }
        }

    private:
        using ArcMap = std::unordered_map<size_t, std::unique_ptr<Arc<ResourceType>>>;
        using NodeMap = std::unordered_map<size_t, std::unique_ptr<Node<ResourceType>>>;

        // Arc storage: indexed directly by arc_id. nullptr slots are removed arcs.
        std::vector<std::unique_ptr<Arc<ResourceType>>> arcs_;
        size_t active_arc_count_ = 0;

        NodeMap nodes_by_id_;
        std::vector<Node<ResourceType>*> sorted_nodes_;
        bool modified_ = false;
        size_t next_arc_id_ = 0;

        ArcMap removed_arcs_by_id_;

        std::vector<size_t> source_node_ids_;
        std::vector<size_t> sink_node_ids_;

        // CSR (Compressed Sparse Row) arc arrays — rebuilt by build_csr() / sort_nodes().
        mutable std::vector<Arc<ResourceType>*> csr_out_arcs_;
        mutable std::vector<Arc<ResourceType>*> csr_in_arcs_;
        mutable bool csr_valid_ = false;

        // Internal helper: insert an arc at a specific slot (used by clone()).
        Arc<ResourceType>& add_arc_at(Node<ResourceType>* origin_node,
                                      Node<ResourceType>* destination_node, double cost,
                                      std::vector<Row> rows, size_t arc_id) {
            next_arc_id_ = std::max(next_arc_id_, arc_id + 1);
            if (arc_id >= arcs_.size()) {
                arcs_.resize(arc_id + 1);
            }
            arcs_[arc_id] = std::make_unique<Arc<ResourceType>>(arc_id,
                                                                origin_node,
                                                                destination_node,
                                                                cost,
                                                                rows);
            ++active_arc_count_;
            modified_ = true;
            csr_valid_ = false;
            origin_node->out_arcs.push_back(arcs_[arc_id].get());
            destination_node->in_arcs.push_back(arcs_[arc_id].get());
            return *arcs_[arc_id];
        }

        // Internal helper: restore one arc while iterating removed_arcs_by_id_.
        typename ArcMap::iterator restore_arc_from_map(typename ArcMap::iterator it) {
            Arc<ResourceType>* arc = it->second.get();
            arc->destination->in_arcs.push_back(arc);
            arc->origin->out_arcs.push_back(arc);
            size_t arc_id = it->first;
            if (arc_id >= arcs_.size()) {
                arcs_.resize(arc_id + 1);
            }
            arcs_[arc_id] = std::move(it->second);
            ++active_arc_count_;
            modified_ = true;
            csr_valid_ = false;
            return removed_arcs_by_id_.erase(it);
        }
};

template <typename ResourceType>
std::ostream& operator<<(std::ostream& os, const Graph<ResourceType>& graph) {
    return os << graph.to_string();
}
}  // namespace rcspp
