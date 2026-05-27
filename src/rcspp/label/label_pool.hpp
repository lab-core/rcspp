// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <memory>
#include <utility>
#include <vector>

#include "rcspp/label/label_factory.hpp"

namespace rcspp {

inline constexpr size_t DEFAULT_LABEL_POOL_SIZE = 1e4;

template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class LabelPool {
    public:
        explicit LabelPool(std::unique_ptr<LabelFactory<ResourceType>> label_factory,
                           size_t initial_size = DEFAULT_LABEL_POOL_SIZE)
            : label_factory_(std::move(label_factory)) {
            labels_.reserve(initial_size);
            available_labels_.reserve(initial_size);
        }

        std::unique_ptr<LabelPool<ResourceType>> clone() {
            return std::make_unique<LabelPool<ResourceType>>(
                std::make_unique<LabelFactory<ResourceType>>(*label_factory_));
        }

        Label<ResourceType>& get_next_label(const Node<ResourceType>* end_node,
                                            const Arc<ResourceType>* in_arc = nullptr,
                                            const Arc<ResourceType>* out_arc = nullptr) {
            // size_t label_id = 0;

            Label<ResourceType>* label_ptr = nullptr;

            if (!available_labels_.empty()) {
                label_ptr = available_labels_.back();
                available_labels_.pop_back();
                label_factory_->reset_label(label_ptr, nb_labels_, end_node, in_arc, out_arc);
                ++nb_reused_labels_;
            } else {
                // A new label is created
                labels_.emplace_back(
                    label_factory_->make_label(nb_labels_, end_node, in_arc, out_arc));
                label_ptr = labels_.back().get();
                ++nb_created_labels_;
            }
            ++nb_labels_;

            return *label_ptr;
        }

        /// @brief Attempt to return a label to the pool.
        ///
        /// If the label still has living children (@ref child_refcount_ > 0) it is left
        /// in place; @ref do_release will cascade-recycle it once the last child is gone.
        void release_label(Label<ResourceType>* label_ptr) {
            if (label_ptr->child_refcount_ == 0) {
                do_release(label_ptr);
            }
            // else: children still alive; marked dominated, not processed,
            // will be recycled by the last child's release path.
        }

        void clear() {
            labels_.clear();
            available_labels_.clear();
        }

        /// @brief Free all label memory and release backing storage to the OS.
        ///
        /// Unlike clear(), which only destroys label objects but retains the
        /// vector capacity for reuse, release() also calls shrink_to_fit() on
        /// both internal vectors.  Use this at the end of a solve to reclaim
        /// RAM when the pool will not be reused immediately.
        void release() {
            labels_.clear();
            labels_.shrink_to_fit();
            available_labels_.clear();
            available_labels_.shrink_to_fit();
        }

        [[nodiscard]] int64_t get_nb_created_labels() const { return nb_created_labels_; }

        [[nodiscard]] int64_t get_nb_reused_labels() const { return nb_reused_labels_; }

    private:
        /// @brief Unconditionally return a label to the free list and cascade to its parent.
        ///
        /// Decrements the parent's @ref child_refcount_ and recurses into the parent when it
        /// becomes zero and is already dominated — avoiding a separate traversal at solve end.
        void do_release(Label<ResourceType>* label) {
            if (label->parent_ != nullptr) {
                auto* parent = label->parent_;
                label->parent_ = nullptr;
                if (--parent->child_refcount_ == 0 && parent->dominated) {
                    do_release(parent);
                }
            }
            available_labels_.push_back(label);
        }

        std::unique_ptr<LabelFactory<ResourceType>> label_factory_;
        std::vector<std::unique_ptr<Label<ResourceType>>> labels_;
        std::vector<Label<ResourceType>*> available_labels_;

        uint64_t nb_labels_{0};
        uint64_t nb_created_labels_{0};
        uint64_t nb_reused_labels_{0};
};
}  // namespace rcspp
