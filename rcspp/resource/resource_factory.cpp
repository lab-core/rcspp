#include "resource_factory.h"

#include "concrete/real_resource.h"


ResourceFactory::ResourceFactory(std::unique_ptr<Resource> resource_prototype) :
  resource_prototype_(std::move(resource_prototype)), nb_labels_created_(0) {

}

std::unique_ptr<Resource> ResourceFactory::make_resource() {

  std::cout << "ResourceFactory::make_resource()\n";

  nb_labels_created_++;
  
  return resource_prototype_->clone();
}