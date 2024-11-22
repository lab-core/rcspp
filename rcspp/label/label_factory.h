#pragma once

#include "label.h"
#include <resource/resource_factory.h>


class LabelFactory {

private:
  ResourceFactory& resource_factory_;

public:

  LabelFactory(ResourceFactory& resource_factory);

  std::unique_ptr<Label> make_label(size_t id, const Node* end_node = nullptr, const Arc* in_arc = nullptr, 
    const Arc* out_arc = nullptr, Label* previous_label = nullptr, Label* next_label = nullptr);

};