#include "arc.h"

#include <iostream>

Arc::Arc(size_t arc_id, const Node& origin_node, const Node& destination_node, std::unique_ptr<Resource> arc_resource) :
  id(arc_id), origin(origin_node), destination(destination_node), resource(std::move(arc_resource)) {

}
