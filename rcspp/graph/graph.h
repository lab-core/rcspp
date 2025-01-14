#pragma once

#include "arc.h"
#include "resource/resource.h"

#include <memory>
#include <vector>
#include <map>
#include <optional>
#include <ranges>


template<typename ResourceType>
class Graph {

public:

  Graph() {}

  Node<ResourceType>& add_node(size_t id, bool source = false, bool sink = false) {

    nodes_by_id_[id] = std::make_unique<Node<ResourceType>>(id);

    if (source) {
      source_node_ids_.push_back(nodes_by_id_[id]->id);
    }

    if (sink) {
      sink_node_ids_.push_back(nodes_by_id_[id]->id);
    }

    return *nodes_by_id_[id];
  }


  Arc<ResourceType>& add_arc(Node<ResourceType>& origin_node, Node<ResourceType>& destination_node, 
    std::unique_ptr<ResourceType> resource, std::optional<size_t> id = std::nullopt) {

    if (id == std::nullopt) {
      id = arcs_by_id_.size();
    }

    arcs_by_id_[*id] = std::make_unique<Arc<ResourceType>>(*id, origin_node, destination_node, std::move(resource));

    auto new_arc_ptr = arcs_by_id_[*id].get();

    origin_node.out_arcs.push_back(new_arc_ptr);
    destination_node.in_arcs.push_back(new_arc_ptr);

    return *new_arc_ptr;
  }

  Arc<ResourceType>& add_arc(size_t origin_node_id, size_t destination_node_id, std::unique_ptr<ResourceType> resource, 
    std::optional<size_t> id = std::nullopt) {

    auto& origin_node = *nodes_by_id_.at(origin_node_id);
    auto& destination_node = *nodes_by_id_.at(destination_node_id);

    return add_arc(origin_node, destination_node, std::move(resource), id);
  }

  Node<ResourceType>& get_node(size_t id) const {

    return *nodes_by_id_.at(id).get();
  }

  Arc<ResourceType>& get_arc(size_t id) const {

    return *arcs_by_id_.at(id).get();
  }

  std::vector<size_t> get_node_ids() const {

    auto node_ids_ranges = std::views::keys(nodes_by_id_);

    return std::vector<size_t>{node_ids_ranges.begin(), node_ids_ranges.end()};
  }

  std::vector<size_t> get_arc_ids() const {

    auto arc_ids_ranges = std::views::keys(arcs_by_id_);

    return std::vector<size_t>{arc_ids_ranges.begin(), arc_ids_ranges.end()};
  }

  std::vector<size_t> get_source_node_ids() const {

    return source_node_ids_;
  }

  std::vector<size_t> get_sink_node_ids() const {

    return sink_node_ids_;
  }

  bool is_source(size_t node_id) const {

    return std::find(source_node_ids_.begin(), source_node_ids_.end(), node_id) != source_node_ids_.end();
  }

  bool is_sink(size_t node_id) const {

    return std::find(sink_node_ids_.begin(), sink_node_ids_.end(), node_id) != sink_node_ids_.end();
  }

private:

  std::map<size_t, std::unique_ptr<Arc<ResourceType>>> arcs_by_id_;
  std::map<size_t, std::unique_ptr<Node<ResourceType>>> nodes_by_id_;

  std::vector<size_t> source_node_ids_;
  std::vector<size_t> sink_node_ids_;

};