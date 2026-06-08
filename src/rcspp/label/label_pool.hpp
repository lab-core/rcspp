// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <memory>
#include <unordered_map>
#include <unordered_set>
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

            // reset also prev label
            label_ptr->prev_label = nullptr;
            label_ptr->ref_count = 0;
            label_ptr->pending_release = false;

            return *label_ptr;
        }

        /// @brief Return a label to the pool's free list.
        void release_label(Label<ResourceType>* label_ptr) {
            available_labels_.push_back(label_ptr);
        }

        /// @brief Release a label, cascading up the prev_label chain as predecessors become free.
        ///
        /// If @p label_ptr is still referenced by alive successors (@ref ref_count > 0), it is
        /// marked @ref pending_release and left in place; the last successor's release will
        /// cascade back up through this method.
        void release_with_ref_count(Label<ResourceType>* label_ptr) {
            while (label_ptr != nullptr) {
                if (label_ptr->ref_count > 0) {
                    label_ptr->pending_release = true;
                    break;
                }
                Label<ResourceType>* prev = label_ptr->prev_label;
                if (prev != nullptr) {
                    --prev->ref_count;
                }
                release_label(label_ptr);
                if (prev == nullptr || !prev->pending_release) {
                    break;
                }
                label_ptr = prev;
            }
        }

        /// @brief Return all labels to the free list without destroying them.
        void release_all_labels() {
            available_labels_.clear();
            for (auto& label_uptr : labels_) {
                available_labels_.push_back(label_uptr.get());
            }
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

        /// @brief Number of labels currently on the free list (available for reuse).
        [[nodiscard]] size_t get_nb_available_labels() const { return available_labels_.size(); }

        /// @brief Total number of label objects owned by the pool (in use + available).
        [[nodiscard]] size_t get_nb_total_labels() const { return labels_.size(); }

        /// @brief Diagnostic: verify the prev_label / ref_count bookkeeping is consistent.
        ///
        /// Considers a label "in use" when it is not on the free list.  For every in-use label
        /// it checks that:
        ///   - its @ref Label::ref_count equals the number of in-use labels that name it as their
        ///     @ref Label::prev_label (no leaked over-count, no missing reference), and
        ///   - its @ref Label::prev_label (when set) points to a label that is itself still in use
        ///     (no dangling predecessor that was already recycled).
        ///
        /// Returns true when both invariants hold for every in-use label.  A false result means a
        /// release path pushed a still-pinned label onto the free list without going through
        /// @ref release_with_ref_count (i.e. it used the raw @ref release_label), leaking the
        /// predecessor's reference or recycling a label that a live successor still points to.
        /// Intended for tests; O(total labels).
        [[nodiscard]] bool check_ref_count_consistency() const {
            std::unordered_set<const Label<ResourceType>*> free_set(available_labels_.begin(),
                                                                    available_labels_.end());
            auto in_use = [&free_set](const Label<ResourceType>* label) {
                return free_set.find(label) == free_set.end();
            };

            // Count, for each label, how many in-use labels reference it as their predecessor.
            std::unordered_map<const Label<ResourceType>*, size_t> referencing_count;
            for (const auto& label_uptr : labels_) {
                const Label<ResourceType>* label = label_uptr.get();
                if (!in_use(label) || label->prev_label == nullptr) {
                    continue;
                }
                if (!in_use(label->prev_label)) {
                    return false;  // dangling: in-use label points to a recycled predecessor
                }
                ++referencing_count[label->prev_label];
            }

            // Every in-use label's ref_count must equal its in-use referrer count.
            for (const auto& label_uptr : labels_) {
                const Label<ResourceType>* label = label_uptr.get();
                if (!in_use(label)) {
                    continue;
                }
                const auto it = referencing_count.find(label);
                const size_t expected = (it == referencing_count.end()) ? 0 : it->second;
                if (label->ref_count != expected) {
                    return false;
                }
            }
            return true;
        }

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
