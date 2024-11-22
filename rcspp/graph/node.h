#pragma once

#include <vector>

class Arc;


class Node {
public:

  Node(size_t node_id);

  const size_t id;

  std::vector<Arc*> in_arcs;
  std::vector<Arc*> out_arcs;
};