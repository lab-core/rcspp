#pragma once

#include "resource.h"

#include <iostream>


template<typename ResourceType>
class ResourceFactory {

public:

  ResourceFactory() {
  
    std::cout << "ResourceFactory\n";

  }

  ResourceFactory(std::unique_ptr<ResourceType> resource_prototype) : 
    resource_prototype_(std::move(resource_prototype)), nb_labels_created_(0) {}

  std::unique_ptr<ResourceType> make_resource() {

    nb_labels_created_++;

    return resource_prototype_->clone();
  }

protected:
  std::unique_ptr<ResourceType> resource_prototype_;

  size_t nb_labels_created_;
};