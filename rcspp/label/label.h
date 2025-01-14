#pragma once

#include "graph/node.h"
#include "graph/arc.h"
#include "resource/resource.h"

#include <memory>
#include <iostream>


template<typename ResourceType>
class Label {
  //friend class LabelFactory<ResourceType>;

private:

  //! Pointer to the node at the end of the path associated with the current label.
  const Node<ResourceType>* end_node_;

  //! Pointer to the arc from which this label was forward expanded.
  const Arc<ResourceType>* in_arc_;

  //! Pointer to the arc from which this label was backward expanded.
  const Arc<ResourceType>* out_arc_;

  //! Pointer to the previous label (used in forward expansion).
  Label* previous_label_;

  //! Pointer to the next label (used in backward expansion).
  Label* next_label_;

  //! Resource consumed by the label.
  std::unique_ptr<ResourceType> resource_;

public:

  Label(size_t label_id, std::unique_ptr<ResourceType> resource) :
    id(label_id), resource_(std::move(resource)), end_node_(nullptr), in_arc_(nullptr), out_arc_(nullptr),
    previous_label_(nullptr), next_label_(nullptr) {

  }

  Label(size_t label_id, std::unique_ptr<ResourceType> resource, const Node<ResourceType>* end_node, const Arc<ResourceType>* in_arc,
    const Arc<ResourceType>* out_arc,
    Label* previous_label, Label* next_label) :
    id(label_id), resource_(std::move(resource)), end_node_(end_node), in_arc_(in_arc), out_arc_(out_arc),
    previous_label_(previous_label), next_label_(next_label) {

  }

  //! Check dominance
  bool operator<=(const Label& rhs_label) const {

    return *resource_ <= *rhs_label.resource_;
  }

  //! Label expansion
  void expand(const Arc<ResourceType>& arc, Label& expanded_label) {

    resource_->expand(*arc.resource, *expanded_label.resource_);
    expanded_label.end_node_ = &arc.destination;
    expanded_label.in_arc_ = &arc;
    expanded_label.out_arc_ = nullptr;
    expanded_label.previous_label_ = this;
    expanded_label.next_label_ = nullptr;
  }

  //! Return label cost
  double get_cost() const {

    return resource_->get_cost();
  }

  //! Return true if the label is feasible
  bool is_feasible() const {

    return resource_->is_feasible();
  }

  //! Return a reference to the node at the end of the path associated with the current label.
  const Node<ResourceType>* get_end_node() const {
    return end_node_;
  }

  //! Return the ids of the nodes corresponding to the path associated with the label.
  std::vector<size_t> get_path_node_ids(bool reverse_order = false) const {

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

  //! Return the ids of the arcs corresponding to the path associated with the label.
  std::vector<size_t> get_path_arc_ids(bool reverse_order = false) const {

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

  //! Label ID
  size_t id;

};