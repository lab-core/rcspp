// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <list>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/label_buckets.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp {

/// @brief The labeling loop, written once against a direction policy and instantiated per
///        direction.
///
/// Everything here except the handful of sites routed through @c Dir is identical in both
/// directions: frontier ordering, dominance bookkeeping, truncation, memory-pressure handling,
/// solution extraction and label pooling.
///
/// The loop helpers take the container set they operate on as a parameter, and their own direction
/// as a template argument, so a bidirectional algorithm can drive a *second* container set through
/// exactly this code rather than mirroring it. Duplicating them would mean duplicating the
/// release_label / release_with_ref_count cascade, which this file's own comments flag as its
/// subtlest logic.
///
/// @tparam ResourceType       The resource type carried by labels.
/// @tparam LabelContainerType The per-node non-dominated label container.
/// @tparam Dir                The direction policy: @ref ForwardDirection or
///                            @ref BackwardDirection.
template <typename ResourceType, typename LabelContainerType, typename Dir>
    requires ResourceTypeConcept<ResourceType>
class DirectionalDominanceAlgorithm : public Algorithm<ResourceType, LabelContainerType> {
        static_assert(DirectionPolicy<Dir, ResourceType>,
                      "Dir does not satisfy DirectionPolicy for this ResourceType");

    public:
        DirectionalDominanceAlgorithm(ResourceFactory<ResourceType>* resource_factory,
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

            for (auto seed_node_id : Dir::seeds(*this->graph_)) {
                auto* seed_node = this->graph_->get_node(seed_node_id);
                auto& label = this->label_pool_.get_next_label(seed_node);
                // Only initial labels keep their starting values; every other label has all of
                // its values overwritten by extension, which is why this is not on the recycling
                // path.
                Dir::seed(label);

                auto& buckets = non_dominated_labels_by_node_pos_.at(seed_node->pos());
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
                if (Dir::is_terminal(label.get_end_node())) {
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
            for (auto arc_ptr : Dir::arcs(*this->graph_, current_node)) {
                extend_label(label_ptr, arc_ptr);
            }
        }

        /// @brief Extends @p label_ptr along @p arc_ptr into this search own containers.
        virtual void extend_label(Label<ResourceType>* label_ptr,
                                  const Arc<ResourceType>* arc_ptr) {
            extend_label<Dir>(label_ptr, arc_ptr, non_dominated_labels_by_node_pos_);
        }

        /// @brief Extends @p label_ptr along @p arc_ptr into an explicit container set.
        ///
        /// @tparam Dir2      The direction to extend in.
        /// @tparam Container The per-node non-dominated label container type.
        /// @param label_ptr  The label to extend.
        /// @param arc_ptr    The arc to extend along.
        /// @param containers The per-node containers to insert the result into.
        template <typename Dir2, typename Container>
        void extend_label(Label<ResourceType>* label_ptr, const Arc<ResourceType>* arc_ptr,
                          std::vector<Container>& containers) {
            // check if arc is not reachable
            if (!label_ptr->is_reachable(Dir2::guard_node_id(*arc_ptr))) {
                return;
            }

            auto* head_node = Dir2::head(*arc_ptr);
            auto& new_label = this->label_pool_.get_next_label(head_node);
            Dir2::extend(*label_ptr, *arc_ptr, &new_label);

            if (++this->num_extended_labels_ % 100000 == 0) {  // NOLINT
                print_labels();
                LOG_DEBUG("Processed ", this->num_extended_labels_, " labels so far...\n");
            }

            bool feasible = Dir2::feasible(new_label);
            if (feasible && update_non_dominated_labels<Dir2>(new_label, containers)) {
                // Add to unprocessed_labels_ and non_dominated_labels_by_node_id_ only if
                // feasible and non dominated.
                // points to the newly inserted element
                auto new_label_it =
                    containers.at(new_label.get_end_node()->pos()).add_label(&new_label);
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

        /// @brief Updates this search own non-dominated set with @p label.
        virtual bool update_non_dominated_labels(const Label<ResourceType>& label) {
            return update_non_dominated_labels<Dir>(label, non_dominated_labels_by_node_pos_);
        }

        /// @brief Updates an explicit non-dominated set with @p label.
        ///
        /// @tparam Dir2      The direction whose dominance order applies.
        /// @tparam Container The per-node non-dominated label container type.
        /// @param label      The candidate label.
        /// @param containers The per-node containers to test against and prune.
        /// @return @c true when @p label is not dominated and should be kept.
        template <typename Dir2, typename Container>
        bool update_non_dominated_labels(const Label<ResourceType>& label,
                                         std::vector<Container>& containers) {
            total_update_non_dom_time_.start();
            ++nb_update_non_dom_iter_;

            auto current_node_pos = label.get_end_node()->pos();
            auto& non_dominated_labels_list = containers.at(current_node_pos);

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

        /// @brief Removes @p label_iterator from this search own containers.
        virtual void remove_label(const std::list<Label<ResourceType>*>::iterator& label_iterator) {
            remove_label<Dir>(label_iterator, non_dominated_labels_by_node_pos_);
        }

        /// @brief Removes @p label_iterator from an explicit container set.
        ///
        /// @tparam Dir2      The direction the containers belong to.
        /// @tparam Container The per-node non-dominated label container type.
        /// @param label_iterator Iterator into the container holding the label.
        /// @param containers The per-node containers to erase from.
        template <typename Dir2, typename Container>
        void remove_label(const std::list<Label<ResourceType>*>::iterator& label_iterator,
                          std::vector<Container>& containers) {
            auto current_node_pos = (*label_iterator)->get_end_node()->pos();
            containers.at(current_node_pos).erase_label(label_iterator);
        }

        [[nodiscard]] std::list<Label<ResourceType>*> get_labels_at_sinks() const override {
            return get_labels_at_terminals<Dir>(non_dominated_labels_by_node_pos_);
        }

        /// @brief Collects the labels sitting at this direction terminal nodes.
        ///
        /// Direction-dependent and load-bearing rather than symmetry for its own sake: a complete
        /// path is recorded either by the loop reaching a terminal or by this sweep of what is
        /// left, so a search that looked at the wrong end would silently lose whole routes.
        ///
        /// @tparam Dir2      The direction whose terminals to sweep.
        /// @tparam Container The per-node non-dominated label container type.
        /// @param containers The per-node containers to read.
        /// @return Every label held at a terminal node of @c Dir2.
        template <typename Dir2, typename Container>
        [[nodiscard]] std::list<Label<ResourceType>*> get_labels_at_terminals(
            const std::vector<Container>& containers) const {
            std::list<Label<ResourceType>*> labels_at_terminals;
            for (auto terminal_node_id : Dir2::terminals(*this->graph_)) {
                auto node_pos = this->graph_->get_node(terminal_node_id)->pos();
                const auto& labels_at_current_terminal = containers.at(node_pos).get_labels();
                labels_at_terminals.insert(labels_at_terminals.end(),
                                           labels_at_current_terminal.begin(),
                                           labels_at_current_terminal.end());
            }

            return labels_at_terminals;
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

}  // namespace rcspp
