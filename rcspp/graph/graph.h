#pragma once

#include "arc.h"
#include "resource/resource.h"

#include <memory>
#include <vector>
#include <map>
#include <optional>


class Graph {

public:

  Graph();

  Node& add_node(size_t id, bool source = false, bool sink = false);

  Arc& add_arc(Node& origin_node, Node& destination_node, std::unique_ptr<Resource> resource, std::optional<size_t> id = std::nullopt);

  Arc& add_arc(size_t origin_node_id, size_t destination_node_id, std::unique_ptr<Resource> resource, std::optional<size_t> id = std::nullopt);

  Node& get_node(size_t id) const;

  Arc& get_arc(size_t id) const;

  std::vector<size_t> get_node_ids() const;

  std::vector<size_t> get_arc_ids() const;

  std::vector<size_t> get_source_node_ids() const;

  std::vector<size_t> get_sink_node_ids() const;

  bool is_source(size_t node_id) const;

  bool is_sink(size_t node_id) const;

private:

  std::map<size_t, std::unique_ptr<Arc>> arcs_by_id_;
  std::map<size_t, std::unique_ptr<Node>> nodes_by_id_;

  std::vector<size_t> source_node_ids_;
  std::vector<size_t> sink_node_ids_;

};