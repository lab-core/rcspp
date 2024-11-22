#pragma once

#include "node.h"
#include "resource/resource.h"

#include <memory>


class Arc {
public:

  Arc(size_t arc_id, const Node& origin_node, const Node& destination_node, std::unique_ptr<Resource> resource);

  const size_t id;

  const Node& origin;

  const Node& destination;

  const std::unique_ptr<Resource> resource;

};
