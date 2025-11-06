// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "rcspp/label/label_factory.hpp"

namespace rcspp {

template <typename ResourceType>
  requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class LabelPool {
  public:
    explicit LabelPool(std::unique_ptr<LabelFactory<ResourceType>> label_factory,
                       bool use_pool = true)
        : label_factory_(std::move(label_factory)),

          use_pool_(use_pool),
          temporary_label_ptr_(nullptr) {}

    Label<ResourceType>& get_next_label(const Node<ResourceType>* end_node,
                                        const Arc<ResourceType>* in_arc = nullptr,
                                        const Arc<ResourceType>* out_arc = nullptr) {
      // size_t label_id = 0;

      Label<ResourceType>* label_ptr = nullptr;

      if (!available_labels_.empty() && use_pool_) {
        label_ptr = available_labels_.back();
        available_labels_.pop_back();

        label_factory_->reset_label(label_ptr, nb_labels_, end_node, in_arc, out_arc);

        nb_labels_++;
        nb_reused_labels_++;
      } else {
        // A new label is created
        auto new_label = label_factory_->make_label(nb_labels_, end_node, in_arc, out_arc);

        labels_.push_back(std::move(new_label));
        label_ptr = labels_.back().get();

        nb_labels_++;
        nb_created_labels_++;
      }

      return *label_ptr;
    }

    Label<ResourceType>& get_temporary_label(const Node<ResourceType>* end_node,
                                             const Arc<ResourceType>* in_arc = nullptr,
                                             const Arc<ResourceType>* out_arc = nullptr) {
      // LOG_TRACE(__FUNCTION__, '\n');

      if (temporary_label_ptr_) {
        // LOG_TRACE("temporary_label_ptr_\n");

        label_factory_->reset_label(temporary_label_ptr_, nb_labels_, end_node, in_arc, out_arc);

        nb_labels_++;
      } else {
        // LOG_TRACE("NOT temporary_label_ptr_\n");

        temporary_label_ptr_ = label_factory_->make_label(nb_labels_, end_node, in_arc, out_arc);

        nb_labels_++;
      }

      return *temporary_label_ptr_;
    }

    void release_label(Label<ResourceType>* label_ptr) { available_labels_.push_back(label_ptr); }

    [[nodiscard]] int64_t get_nb_created_labels() const { return nb_created_labels_; }

    [[nodiscard]] int64_t get_nb_reused_labels() const { return nb_reused_labels_; }

  private:
    std::unique_ptr<LabelFactory<ResourceType>> label_factory_;

    bool use_pool_;

    std::vector<std::unique_ptr<Label<ResourceType>>> labels_;

    // std::map<size_t, std::unique_ptr<Label<ResourceType>>> labels_;

    std::vector<Label<ResourceType>*> available_labels_;
    // std::vector<size_t> available_label_ids_;
    // std::set<size_t> available_label_ids_;

    std::unique_ptr<Label<ResourceType>> temporary_label_ptr_;

    size_t nb_labels_{0};

    int64_t nb_created_labels_{0};
    int64_t nb_reused_labels_{0};
};
}  // namespace rcspp
