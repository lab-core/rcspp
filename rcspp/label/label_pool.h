#pragma once

#include "label_factory.h"

#include <memory>
#include <optional>


template<typename ResourceType>
class LabelPool {
public:
  LabelPool(std::unique_ptr<LabelFactory<ResourceType>> label_factory, std::optional<int> size = std::nullopt) :
    label_factory_(std::move(label_factory)), size_(size), next_label_id_(0) { }

  Label<ResourceType>& get_next_label(const Node<ResourceType>* end_node = nullptr, const Arc<ResourceType>* in_arc = nullptr,
    const Arc<ResourceType>* out_arc = nullptr, Label<ResourceType>* previous_label = nullptr, 
    Label<ResourceType>* next_label = nullptr) {

    size_t next_label_index = 0;

    if (size_ == std::nullopt) {
      // A new label is created every time (i.e., no pool is used)
      auto new_label = label_factory_->make_label(next_label_id_, end_node, in_arc, out_arc, previous_label, next_label);

      labels_.push_back(std::move(new_label));

      next_label_id_++;
      next_label_index = labels_.size() - 1;
    }
    else {
      // TO DO: To be implemented
    }

    return *labels_[next_label_index];
  }

private:
  std::unique_ptr<LabelFactory<ResourceType>> label_factory_;

  std::optional<int> size_;

  std::vector<std::unique_ptr<Label<ResourceType>>> labels_;

  size_t next_label_id_;
};