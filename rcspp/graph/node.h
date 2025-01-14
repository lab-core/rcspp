#pragma once

#include <vector>

template<typename ResourceType>
class Arc;

template<typename ResourceType>
class Node {
public:

  Node(size_t node_id) : id(node_id) {

  };

  const size_t id;

  std::vector<Arc<ResourceType>*> in_arcs;
  std::vector<Arc<ResourceType>*> out_arcs;
};