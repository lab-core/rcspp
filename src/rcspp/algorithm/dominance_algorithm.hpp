// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <list>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/label_buckets.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp {

template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>>
    requires ResourceTypeConcept<ResourceType>
class DominanceAlgorithm : public Algorithm<ResourceType, LabelContainerType> {
    public:
        DominanceAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                           AlgorithmParams<LabelContainerType> params)
            : Algorithm<ResourceType, LabelContainerType>(resource_factory, std::move(params)) {}

    protected:
        /// @brief Release label memory and clear the non-dominated label containers.
        ///
        /// Overrides @ref Algorithm::release_label_memory() to also clear
        /// @ref non_dominated_labels_by_node_pos_ so that no dangling label
        /// pointers remain after the pool is freed.  Subclasses that own
        /// additional label containers (e.g. unprocessed queues) should
        /// override this further and call this base implementation.
        void release_label_memory() override {
            Algorithm<ResourceType, LabelContainerType>::release_label_memory();
            non_dominated_labels_by_node_pos_.clear();
        }

        void initialize_labels() override {
            // Release all labels from the previous run (including any pending_release ones)
            // so the pool is fully reset before we start fresh.
            this->label_pool_.release_all_labels();

            non_dominated_labels_by_node_pos_.clear();
            non_dominated_labels_by_node_pos_.reserve(this->graph_->get_number_of_nodes());
            for (size_t i = 0; i < this->graph_->get_number_of_nodes(); i++) {
                non_dominated_labels_by_node_pos_.emplace_back(this->params_.labels.copy());
            }

            for (auto source_node_id : this->graph_->get_source_node_ids()) {
                auto* source_node = this->graph_->get_node(source_node_id);
                auto& label = this->label_pool_.get_next_label(source_node);

                auto& buckets = non_dominated_labels_by_node_pos_.at(source_node->pos());
                // it points to the newly inserted element
                auto label_it = buckets.add_label(&label);
                add_new_unprocessed_label(std::make_pair(&label, label_it));
            }
        }

        void main_loop() override {  // NOLINT
            size_t i = 0;
            while (this->number_of_labels() > 0 && !this->should_stop(i)) {
                // Periodic memory check (skip i == 0 to avoid cost on every first iteration).
                if (i > 0 && this->memory_limit_.effective_limit > 0 &&
                    i % this->params_.memory_check_interval == 0) {
                    if (this->memory_limit_.is_exceeded()) {
                        LOG_WARN("Memory limit (",
                                 this->memory_limit_.effective_limit / (1024ULL * 1024ULL),
                                 " MB) exceeded (current: ",
                                 MemoryInfo::process_bytes() / (1024ULL * 1024ULL),
                                 " MB). Stopping early.\n");
                        break;
                    }
                    if (this->memory_limit_.is_under_pressure()) {
                        LOG_INFO("Memory pressure: ",
                                 MemoryInfo::process_bytes() / (1024ULL * 1024ULL),
                                 " MB / ",
                                 this->memory_limit_.effective_limit / (1024ULL * 1024ULL),
                                 " MB. Trimming label queues.\n");
                        this->on_memory_pressure();
                    }
                }

                ++i;

                // next label to process
                auto label_iterator_pair = next_label_iterator();

                // no more label -> break (useful when pulling)
                if (label_iterator_pair.first == nullptr) {
                    break;
                }

                // label dominated -> continue to next one
                auto& label = *label_iterator_pair.first;
                if (label.dominated) {
                    this->label_pool_.release_with_ref_count(&label);
                    continue;
                }
                if (this->params_.prune_based_on_upper_bound_ &&
                    label.get_cost() >= this->best_cost_upper_bound_) {
                    remove_label(label_iterator_pair.second);
                    // Use release_with_ref_count (not release_label): this label was added to
                    // the non-dominated set, so it pins a predecessor whose ref_count must be
                    // decremented. Plain release_label would leak that predecessor.
                    this->label_pool_.release_with_ref_count(&label);
                    continue;
                }

                assert(label.get_end_node());

                // check if we can update the best label or extend
                if (label.get_end_node()->sink) {
                    if (label.get_cost() < this->cost_upper_bound_) {
                        if (label.get_cost() < this->best_cost_upper_bound_) {
                            this->best_cost_upper_bound_ = label.get_cost();
                        }
                        if (this->params_.return_dominated_solutions) {
                            this->extract_solution(label);
                            if (this->solutions_.size() >= this->params_.stop_after_X_solutions) {
                                LOG_DEBUG("Stopping after ",
                                          this->solutions_.size(),
                                          " solutions.\n");
                                return;
                            }
                        }
                    }
                } else if (!std::isinf(label.get_cost())) {
                    this->total_full_extend_time_.start();
                    this->extend(&label);
                    this->total_full_extend_time_.stop();
                } else {
                    remove_label(label_iterator_pair.second);
                    this->label_pool_.release_with_ref_count(&label);
                }
            }
        }

        virtual LabelIteratorPair<ResourceType> next_label_iterator() = 0;

        virtual void extend(Label<ResourceType>* label_ptr) {
            const auto& current_node = label_ptr->get_end_node();
            for (auto arc_ptr : this->graph_->get_out_arcs(current_node)) {
                extend_label(label_ptr, arc_ptr);
            }
        }

        virtual void extend_label(Label<ResourceType>* label_ptr,
                                  const Arc<ResourceType>* arc_ptr) {
            // check if arc is not reachable
            if (!label_ptr->is_reachable(arc_ptr->destination->id)) {
                return;
            }

            auto& new_label = this->label_pool_.get_next_label(arc_ptr->destination);
            label_ptr->extend(*arc_ptr, &new_label);

            if (++this->num_extended_labels_ % 100000 == 0) {  // NOLINT
                print_labels();
                LOG_DEBUG("Processed ", this->num_extended_labels_, " labels so far...\n");
            }

            bool feasible = new_label.is_feasible();
            if (feasible && update_non_dominated_labels(new_label)) {
                // Add to unprocessed_labels_ and non_dominated_labels_by_node_id_ only if
                // feasible and non dominated.
                // points to the newly inserted element
                auto new_label_it =
                    non_dominated_labels_by_node_pos_.at(new_label.get_end_node()->pos())
                        .add_label(&new_label);
                add_new_unprocessed_label(std::make_pair(&new_label, new_label_it));
                // Pin predecessor: keep it alive until this label is released.
                new_label.set_prev_label(label_ptr);
            } else {
                if (!feasible) {
                    ++this->nb_infeasible_labels_;
                } else {
                    ++this->nb_dominated_labels_;
                }
                // new_label was never a predecessor; release immediately.
                this->label_pool_.release_label(&new_label);
            }
        }

        /// @brief Reconstruct the path by following prev_label pointers.
        ///
        /// O(hops): every accepted label stores a pointer to its predecessor,
        /// kept alive via @ref ref_count until this label is released.
        std::vector<size_t> get_path_arc_ids(const Label<ResourceType>& label) override {
            std::vector<size_t> path_arc_ids;
            const Label<ResourceType>* cur = &label;
            while (cur != nullptr && cur->get_in_arc() != nullptr) {
                path_arc_ids.push_back(cur->get_in_arc()->id);
                cur = cur->prev_label;
            }
            std::ranges::reverse(path_arc_ids);
            return path_arc_ids;
        }

        virtual bool update_non_dominated_labels(const Label<ResourceType>& label) {
            total_update_non_dom_time_.start();
            ++nb_update_non_dom_iter_;

            auto current_node_pos = label.get_end_node()->pos();
            auto& non_dominated_labels_list =
                non_dominated_labels_by_node_pos_.at(current_node_pos);

            // First, check if label is dominated by any existing non-dominated label
            bool label_dominated = non_dominated_labels_list.is_dominated(label);
            if (label_dominated) {
                total_update_non_dom_time_.stop();
                return false;
            }

            // Second, remove all existing labels that are dominated by label
            non_dominated_labels_list.remove_dominated_labels(label);

            total_update_non_dom_time_.stop();

            return true;
        }

        virtual void remove_label(const std::list<Label<ResourceType>*>::iterator& label_iterator) {
            auto current_node_pos = (*label_iterator)->get_end_node()->pos();
            non_dominated_labels_by_node_pos_.at(current_node_pos).erase_label(label_iterator);
        }

        [[nodiscard]] std::list<Label<ResourceType>*> get_labels_at_sinks() const override {
            std::list<Label<ResourceType>*> labels_at_sinks;
            for (auto sink_node_id : this->graph_->get_sink_node_ids()) {
                auto node_pos = this->graph_->get_node(sink_node_id)->pos();
                const auto& labels_at_current_sink =
                    non_dominated_labels_by_node_pos_.at(node_pos).get_labels();
                labels_at_sinks.insert(labels_at_sinks.end(),
                                       labels_at_current_sink.begin(),
                                       labels_at_current_sink.end());
            }

            return labels_at_sinks;
        }

        void print_labels() const override {
            if (LOG_TRACE_ACTIVE()) {
                LOG_TRACE("All non dominated labels by node:\n");
                for (size_t pos = 0; pos < non_dominated_labels_by_node_pos_.size(); pos++) {
                    LOG_TRACE("Node ", this->graph_->get_sorted_nodes().at(pos)->id, ":\n");
                    non_dominated_labels_by_node_pos_.at(pos).print_labels();
                }
            }
        }

        virtual void add_new_unprocessed_label(
            const LabelIteratorPair<ResourceType>& label_iterator_pair) = 0;

        std::vector<LabelContainerType> non_dominated_labels_by_node_pos_;

        Timer total_extend_time_;
        Timer total_update_non_dom_time_;

        size_t nb_infeasible_labels_ = 0;
        size_t nb_update_non_dom_iter_ = 0;
        size_t nb_extend_iter_ = 0;
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
