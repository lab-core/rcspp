#pragma once

#include "resource.h"


class ResourceFactory {

public:

  ResourceFactory(std::unique_ptr<Resource> resource_prototype);

  std::unique_ptr<Resource> make_resource();

protected:
  std::unique_ptr<Resource> resource_prototype_;

  size_t nb_labels_created_;
};