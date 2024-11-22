#pragma once

#include "label_factory.h"

#include <memory>
#include <optional>


class LabelPool {
public:
  LabelPool(std::unique_ptr<LabelFactory> label_factory_, std::optional<int> size = std::nullopt);

  Label& get_next_label(const Node* end_node = nullptr, const Arc* in_arc = nullptr, const Arc* out_arc = nullptr,
    Label* previous_label = nullptr, Label* next_label = nullptr);

private:
  std::unique_ptr<LabelFactory> label_factory_;

  std::optional<int> size_;

  std::vector<std::unique_ptr<Label>> labels_;

  size_t next_label_id_;
};