#pragma once

#include "label.h"
#include <resource/resource_factory.h>


template<typename ResourceType>
class LabelFactory {

private:
  ResourceFactory<ResourceType>& resource_factory_;

public:

  LabelFactory(ResourceFactory<ResourceType>& resource_factory) : resource_factory_(resource_factory) { }

  std::unique_ptr<Label<ResourceType>> make_label(size_t id, const Node<ResourceType>* end_node = nullptr, 
    const Arc<ResourceType>* in_arc = nullptr, const Arc<ResourceType>* out_arc = nullptr, 
    Label<ResourceType>* previous_label = nullptr, Label<ResourceType>* next_label = nullptr) {

    auto resource = resource_factory_.make_resource();

    return std::make_unique<Label<ResourceType>>(id, std::move(resource), end_node, in_arc, out_arc, previous_label, next_label);
  }

};