#pragma once

#include "graph/node.h"
#include "graph/arc.h"
#include "resource/resource.h"

#include <memory>


class Label {
  friend class LabelFactory;

private:

  //! Pointer to the node at the end of the path associated with the current label.
  const Node* end_node_;

  //! Pointer to the arc from which this label was forward expanded.
  const Arc* in_arc_;

  //! Pointer to the arc from which this label was backward expanded.
  const Arc* out_arc_;

  //! Pointer to the previous label (used in forward expansion).
  Label* previous_label_;

  //! Pointer to the next label (used in backward expansion).
  Label* next_label_;

  //! Resource consumed by the label.
  std::unique_ptr<Resource> resource_;

public:

  Label(size_t label_id, std::unique_ptr<Resource> resource);

  Label(size_t label_id, std::unique_ptr<Resource> resource, const Node* end_node, const Arc* in_arc, const Arc* out_arc,
    Label* previous_label, Label* next_label);

  //! Check dominance
  bool operator<=(const Label& rhs_label) const;

  //! Label expansion
  void expand(const Arc& arc, Label& expanded_label);

  //! Return label cost
  double get_cost() const;

  //! Return true if the label is feasible
  bool is_feasible() const;

  //! Return a reference to the node at the end of the path associated with the current label.
  const Node* get_end_node() const;

  //! Return the ids of the nodes corresponding to the path associated with the label.
  std::vector<size_t> get_path_node_ids(bool reverse_order = false) const;

  //! Return the ids of the arcs corresponding to the path associated with the label.
  std::vector<size_t> get_path_arc_ids(bool reverse_order = false) const;

  //! Label ID
  size_t id;

};