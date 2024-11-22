#include "label.h"

#include <memory>

#include <iostream>


Label::Label(size_t label_id, std::unique_ptr<Resource> resource) : 
  id(label_id), resource_(std::move(resource)), end_node_(nullptr), in_arc_(nullptr), out_arc_(nullptr), 
  previous_label_(nullptr), next_label_(nullptr) {

}

Label::Label(size_t label_id, std::unique_ptr<Resource> resource, const Node* end_node, const Arc* in_arc, 
  const Arc* out_arc, Label* previous_label, Label* next_label) :
  id(label_id), resource_(std::move(resource)), end_node_(end_node), in_arc_(in_arc), out_arc_(out_arc),
  previous_label_(previous_label), next_label_(next_label) {

}

bool Label::operator<=(const Label& rhs_label) const {
  
  return *resource_ <= *rhs_label.resource_;
}

void Label::expand(const Arc& arc, Label& expanded_label) {

  resource_->expand(*arc.resource, *expanded_label.resource_);
  expanded_label.end_node_ = &arc.destination;
  expanded_label.in_arc_ = &arc;
  expanded_label.out_arc_ = nullptr;
  expanded_label.previous_label_ = this;
  expanded_label.next_label_ = nullptr;
}

double Label::get_cost() const {

  return resource_->get_cost();
}

bool Label::is_feasible() const {

  return resource_->is_feasible();
}

const Node* Label::get_end_node() const {
  return end_node_;
}

std::vector<size_t> Label::get_path_node_ids(bool reverse_order) const {

  std::vector<size_t> path_node_ids;

  auto label = this;
  while (label != nullptr) {

    path_node_ids.push_back(label->end_node_->id);
    
    label = label->previous_label_;
  }

  if (!reverse_order) {
    std::reverse(path_node_ids.begin(), path_node_ids.end());
  }  

  return path_node_ids;
}

std::vector<size_t> Label::get_path_arc_ids(bool reverse_order) const {

  std::vector<size_t> path_arc_ids;

  auto label = this;
  while (label->in_arc_ != nullptr) {

    std::cout << "label->in_arc_=" << label->in_arc_ << std::endl;

    path_arc_ids.push_back(label->in_arc_->id);

    label = label->previous_label_;
  }

  if (!reverse_order) {
    std::reverse(path_arc_ids.begin(), path_arc_ids.end());
  }

  return path_arc_ids;
}