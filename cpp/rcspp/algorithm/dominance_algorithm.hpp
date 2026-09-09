// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <list>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/directional_dominance_algorithm.hpp"
#include "rcspp/algorithm/label_buckets.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp {

/// @brief The forward labeling loop: @ref DirectionalDominanceAlgorithm bound to
///        @ref ForwardDirection.
///
/// Exactly two template parameters, deliberately. Two entry points accept only
/// `template <typename, typename> class` -- @c ResourceGraph::solve and the pybind dispatch table
/// -- and the concrete algorithms below are what those bind, so a defaulted third parameter here
/// would force the same `AStarAlgoBound`-style wrapper the codebase already needs elsewhere.
///
/// @tparam ResourceType       The resource type carried by labels.
/// @tparam LabelContainerType The per-node non-dominated label container.
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>>
    requires ResourceTypeConcept<ResourceType>
class DominanceAlgorithm
    : public DirectionalDominanceAlgorithm<ResourceType, LabelContainerType, ForwardDirection> {
    public:
        using DirectionalDominanceAlgorithm<ResourceType, LabelContainerType,
                                            ForwardDirection>::DirectionalDominanceAlgorithm;
};

template <typename ResourceType>
struct NodeUnprocessedLabelsManager {
        void initialize_unprocessed_labels(size_t num_nodes) {
            if (unprocessed_labels_by_node_pos_.empty()) {
                for (size_t i = 0; i < num_nodes; i++) {
                    unprocessed_labels_by_node_pos_.push_back(
                        std::list<LabelIteratorPair<ResourceType>>());
                    truncated_unprocessed_labels_by_node_pos_.push_back(
                        std::list<LabelIteratorPair<ResourceType>>());
                }
            }
            // save unprocessed labels for the current node
            unprocessed_labels_by_node_pos_.at(current_unprocessed_node_pos_)
                .splice(unprocessed_labels_by_node_pos_.at(current_unprocessed_node_pos_).end(),
                        current_unprocessed_labels_);
            // restart the loop at the beginning
            current_unprocessed_node_pos_ = 0;
            this->current_unprocessed_labels_ = std::move(unprocessed_labels_by_node_pos_.at(0));
        }

        void add_new_label(const LabelIteratorPair<ResourceType>& label_iterator_pair) {
            assert(check_number_of_unprocessed_labels());
            size_t pos = label_iterator_pair.first->get_end_node()->pos();
            if (pos == current_unprocessed_node_pos_) {
                current_unprocessed_labels_.push_back(label_iterator_pair);
            } else {
                unprocessed_labels_by_node_pos_.at(pos).push_back(label_iterator_pair);
            }
            ++num_unprocessed_labels_;
        }

        void resize_current_unprocessed_labels(size_t new_size,
                                               LabelPool<ResourceType>* label_pool = nullptr,
                                               bool sort = true) {
            assert(check_number_of_unprocessed_labels());
            resize_unprocessed_labels(&current_unprocessed_labels_, new_size, label_pool, sort);
        }

        void resize_unprocessed_labels(
            std::list<LabelIteratorPair<ResourceType>>* unprocessed_labels, size_t new_size,
            LabelPool<ResourceType>* label_pool, bool sort) {
            if (unprocessed_labels->size() <= new_size) {
                return;
            }
            size_t num_exceeding_labels = unprocessed_labels->size() - new_size;

            if (sort) {
                // sort labels by cost (ascending)
                unprocessed_labels->sort([](const LabelIteratorPair<ResourceType>& p1,
                                            const LabelIteratorPair<ResourceType>& p2) {
                    // either both dominated or both non-dominated
                    if (p1.first->dominated == p2.first->dominated) {
                        return p1.first->get_cost() < p2.first->get_cost();  // lower cost first
                    }
                    return !p1.first->dominated;  // non-dominated first
                });
            }

            // release the exceeding labels
            size_t i = 0;
            for (auto& p : *unprocessed_labels) {
                if (i++ >= new_size) {
                    if (p.first->dominated && label_pool) {
                        label_pool->release_with_ref_count(p.first);
                        p.first = nullptr;
                    } else {
                        store_truncated_unprocessed_label(p);
                    }
                }
            }

            // update unprocessed labels count and resize
            num_unprocessed_labels_ -= num_exceeding_labels;
            unprocessed_labels->resize(new_size);
            assert(check_number_of_unprocessed_labels());
        }

        void store_truncated_unprocessed_label(
            LabelIteratorPair<ResourceType> label_iterator_pair) {
            truncated_unprocessed_labels_by_node_pos_
                .at(label_iterator_pair.first->get_end_node()->pos())
                .push_back(std::move(label_iterator_pair));
        }

        void restore_truncated_unprocessed_labels() {
            size_t pos = 0;
            for (auto& truncated_labels : truncated_unprocessed_labels_by_node_pos_) {
                num_unprocessed_labels_ += truncated_labels.size();
                auto& unprocessed_labels = unprocessed_labels_by_node_pos_.at(pos++);
                unprocessed_labels.splice(unprocessed_labels.end(), truncated_labels);
            }
            // restart the loop at the beginning
            initialize_unprocessed_labels(unprocessed_labels_by_node_pos_.size());
            assert(check_number_of_unprocessed_labels());
        }

        /// @brief Trim all per-node unprocessed queues to at most max_per_node labels.
        ///
        /// Dominated excess labels are immediately recycled into @p pool.
        /// Non-dominated excess labels are stored in the truncated queue for a
        /// subsequent phase, consistent with resize_unprocessed_labels().
        ///
        /// @param max_per_node Maximum labels to retain per node (cheapest ones).
        /// @param pool  Label pool to recycle dominated labels into. May be nullptr.
        void trim_all_queues(size_t max_per_node, LabelPool<ResourceType>* pool) {
            resize_current_unprocessed_labels(max_per_node, pool);
            for (auto& labels_at_node : unprocessed_labels_by_node_pos_) {
                resize_unprocessed_labels(&labels_at_node, max_per_node, pool, /*sort=*/true);
            }
        }

        /// @brief Release and discard all labels currently in the truncated queue.
        ///
        /// For each truncated label: invokes @p remove_from_nondom (a callable with
        /// signature `void(const std::list<Label<ResourceType>*>::iterator&)`) to
        /// remove it from the non-dominated container, then recycles it into @p pool.
        /// Clears the truncated queues afterwards.
        ///
        /// Truncated labels are not counted in @ref num_unprocessed_labels_, so no
        /// counter update is needed.
        ///
        /// @param pool              Pool to recycle labels into.
        /// @param remove_from_nondom  Callable that removes a label from its node's
        ///                            non-dominated set given the list iterator.
        template <typename RemoveFn>
        void release_truncated_labels(LabelPool<ResourceType>* pool,
                                      RemoveFn&& remove_from_nondom) {
            for (auto& truncated_list : truncated_unprocessed_labels_by_node_pos_) {
                for (auto& [label_ptr, label_iter] : truncated_list) {
                    remove_from_nondom(label_iter);
                    // Truncated labels were non-dominated (added to the set), so they pin a
                    // predecessor and may themselves be pinned: release_with_ref_count keeps the
                    // ref_count chain balanced instead of leaking it.
                    pool->release_with_ref_count(label_ptr);
                }
                truncated_list.clear();
            }
        }

        /// @brief Clear all unprocessed and truncated queues.
        ///
        /// Does NOT release the Label objects (pool owns them).  Call this
        /// after the pool has been freed so no dangling pointers remain in
        /// the queues.
        void clear_all_queues() {
            current_unprocessed_labels_.clear();
            for (auto& labels : unprocessed_labels_by_node_pos_) {
                labels.clear();
            }
            for (auto& labels : truncated_unprocessed_labels_by_node_pos_) {
                labels.clear();
            }
            num_unprocessed_labels_ = 0;
            current_unprocessed_node_pos_ = 0;
        }

        [[nodiscard]] bool check_number_of_unprocessed_labels() const {
            size_t total_labels = 0;
            for (const auto& labels_at_node : unprocessed_labels_by_node_pos_) {
                total_labels += labels_at_node.size();
            }
            total_labels += current_unprocessed_labels_.size();
            if (total_labels != num_unprocessed_labels_) {
                LOG_ERROR("Mismatch in number of unprocessed labels: counted ",
                          total_labels,
                          " vs stored ",
                          num_unprocessed_labels_,
                          "\n");
                return false;
            }
            return true;
        }

        size_t num_unprocessed_labels_ = 0;
        size_t current_unprocessed_node_pos_ = 0;
        size_t num_loops_ = 0;
        std::list<LabelIteratorPair<ResourceType>> current_unprocessed_labels_;
        std::vector<std::list<LabelIteratorPair<ResourceType>>> unprocessed_labels_by_node_pos_;
        std::vector<std::list<LabelIteratorPair<ResourceType>>>
            truncated_unprocessed_labels_by_node_pos_;
};
}  // namespace rcspp
