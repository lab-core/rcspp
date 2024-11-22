#include "label_pool.h"


LabelPool::LabelPool(std::unique_ptr<LabelFactory> label_factory, std::optional<int> size) : 
  label_factory_(std::move(label_factory)), size_(size), next_label_id_(0) {

}

Label& LabelPool::get_next_label(const Node* end_node, const Arc* in_arc, const Arc* out_arc,
  Label* previous_label, Label* next_label) {

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