// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "rcspp/label/label_factory.hpp"

namespace rcspp {

/// @brief The number of labels a new pool reserves its free list for. Small, as the first block
///        is: the free list grows as labels are released.
inline constexpr size_t DEFAULT_LABEL_POOL_SIZE = 16;

/// @brief Owns the labels of an algorithm, and hands them out, new or released.
///
/// Labels are constructed in place in blocks, each a vector reserved once and never grown, so that
/// a label never moves and a new label costs no allocation of its own (its resource state still
/// does). The blocks grow: the first holds @ref kFirstBlockLabels labels and each next one twice as
/// many as the one before, up to @ref kMaxBlockLabels. A pool that makes a few labels therefore
/// takes a small block, and one that makes millions allocates once per @ref kMaxBlockLabels. A
/// released label goes to a free list and is reused before a new one is made.
///
/// @tparam ResourceType The resource type carried by labels.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class LabelPool {
    public:
        /// @brief How many labels the first block holds.
        static constexpr size_t kFirstBlockLabels = 16;
        /// @brief The most labels a block holds: blocks double up to it.
        static constexpr size_t kMaxBlockLabels = 4096;

        /// @param label_factory The factory that makes the labels' resource state.
        /// @param initial_size  The number of labels the free list is first reserved for.
        explicit LabelPool(std::unique_ptr<LabelFactory<ResourceType>> label_factory,
                           size_t initial_size = DEFAULT_LABEL_POOL_SIZE)
            : label_factory_(std::move(label_factory)) {
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
                label_ptr = &emplace_label(end_node, in_arc, out_arc);
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
            available_labels_.reserve(get_nb_total_labels());
            for (auto& block : blocks_) {
                for (auto& label : block) {
                    available_labels_.push_back(&label);
                }
            }
        }

        /// @brief Destroy every label, keeping the blocks' memory for the labels to come.
        void clear() {
            for (auto& block : blocks_) {
                block.clear();
            }
            current_block_ = 0;
            available_labels_.clear();
        }

        /// @brief Free all label memory and release backing storage to the OS.
        ///
        /// Unlike clear(), which only destroys label objects but retains the
        /// vector capacity for reuse, release() also calls shrink_to_fit() on
        /// both internal vectors.  Use this at the end of a solve to reclaim
        /// RAM when the pool will not be reused immediately.
        void release() {
            blocks_.clear();
            blocks_.shrink_to_fit();
            current_block_ = 0;
            available_labels_.clear();
            available_labels_.shrink_to_fit();
        }

        [[nodiscard]] int64_t get_nb_created_labels() const { return nb_created_labels_; }

        [[nodiscard]] int64_t get_nb_reused_labels() const { return nb_reused_labels_; }

        /// @brief Number of labels currently on the free list (available for reuse).
        [[nodiscard]] size_t get_nb_available_labels() const { return available_labels_.size(); }

        /// @brief Total number of label objects owned by the pool (in use + available).
        [[nodiscard]] size_t get_nb_total_labels() const {
            size_t total = 0;
            for (const auto& block : blocks_) {
                total += block.size();
            }
            return total;
        }

        /// @brief How many labels the pool's blocks hold without allocating: the labels it owns
        ///        and the room left in its blocks.
        [[nodiscard]] size_t get_label_capacity() const {
            size_t capacity = 0;
            for (const auto& block : blocks_) {
                capacity += block.capacity();
            }
            return capacity;
        }

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
            for (const auto& block : blocks_) {
                for (const auto& label : block) {
                    if (!in_use(&label) || label.prev_label == nullptr) {
                        continue;
                    }
                    if (!in_use(label.prev_label)) {
                        return false;  // dangling: in-use label points to a recycled predecessor
                    }
                    ++referencing_count[label.prev_label];
                }
            }

            // Every in-use label's ref_count must equal its in-use referrer count.
            for (const auto& block : blocks_) {
                for (const auto& label : block) {
                    if (!in_use(&label)) {
                        continue;
                    }
                    const auto it = referencing_count.find(&label);
                    const size_t expected = (it == referencing_count.end()) ? 0 : it->second;
                    if (label.ref_count != expected) {
                        return false;
                    }
                }
            }
            return true;
        }

    private:
        /// @brief Constructs a new label in the first block with room, adding a block when
        ///        every block is full.
        ///
        /// A block is reserved to @ref next_block_capacity when added and filled up to its
        /// capacity only, so it never reallocates and its labels never move.
        Label<ResourceType>& emplace_label(const Node<ResourceType>* end_node,
                                           const Arc<ResourceType>* in_arc,
                                           const Arc<ResourceType>* out_arc) {
            while (current_block_ < blocks_.size() &&
                   blocks_[current_block_].size() == blocks_[current_block_].capacity()) {
                ++current_block_;
            }
            if (current_block_ == blocks_.size()) {
                // Reserved before it joins the pool: an allocation that fails leaves the pool as
                // it was.
                std::vector<Label<ResourceType>> block;
                block.reserve(next_block_capacity());
                blocks_.push_back(std::move(block));
            }
            return blocks_[current_block_].emplace_back(nb_labels_,
                                                        label_factory_->make_resource(*end_node),
                                                        end_node,
                                                        in_arc,
                                                        out_arc);
        }

        /// @brief The capacity of the next block: @ref kFirstBlockLabels for the first, then twice
        ///        the last block's, up to @ref kMaxBlockLabels.
        [[nodiscard]] size_t next_block_capacity() const {
            if (blocks_.empty()) {
                return kFirstBlockLabels;
            }
            return std::min(2 * blocks_.back().capacity(), kMaxBlockLabels);
        }

        std::unique_ptr<LabelFactory<ResourceType>> label_factory_;
        // The labels, in blocks that never reallocate (see emplace_label). Moving a block (when
        // blocks_ grows) moves its buffer, not its labels.
        std::vector<std::vector<Label<ResourceType>>> blocks_;
        // The first block that may have room; the blocks before it are full.
        size_t current_block_ = 0;
        std::vector<Label<ResourceType>*> available_labels_;

        uint64_t nb_labels_{0};
        uint64_t nb_created_labels_{0};
        uint64_t nb_reused_labels_{0};
};
}  // namespace rcspp
