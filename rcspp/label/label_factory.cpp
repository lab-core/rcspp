#include "label_factory.h"

#include <memory>

LabelFactory::LabelFactory(ResourceFactory& resource_factory) : resource_factory_(resource_factory) {

}

std::unique_ptr<Label> LabelFactory::make_label(size_t id, const Node* end_node, const Arc* in_arc, const Arc* out_arc,
  Label* previous_label, Label* next_label) {

  auto resource = resource_factory_.make_resource();

  return std::make_unique<Label>(id, std::move(resource), end_node, in_arc, out_arc, previous_label, next_label);
}