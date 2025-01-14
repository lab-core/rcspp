#pragma once

#include "node.h"
#include "resource/resource.h"

#include <memory>


template<typename ResourceType>
class Arc {
public:

  Arc(size_t arc_id, const Node<ResourceType>& origin_node, const Node<ResourceType>& destination_node, 
    std::unique_ptr<ResourceType> arc_resource) :
    id(arc_id), origin(origin_node), destination(destination_node), resource(std::move(arc_resource)) { }

  const size_t id;

  const Node<ResourceType>& origin;

  const Node<ResourceType>& destination;

  const std::unique_ptr<ResourceType> resource;

};
