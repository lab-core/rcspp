// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cassert>
#include <list>
#include <type_traits>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/container_direction.hpp"
#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/label_buckets.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp {

/// @brief The labeling loop, written once for both search directions.
///
/// Every forward algorithm derives from it with the default direction; a backward search uses it
/// with @ref BackwardDirection. Wherever the two directions differ, the loop asks @p Direction. The
/// templated helpers take their direction and container set as parameters, so a bidirectional
/// algorithm can drive a second container set through the same code.
///
/// @tparam ResourceType       The resource type carried by labels.
/// @tparam LabelContainerType The per-node non-dominated label container its params hold; a
///                            backward search keeps it turned backward (@c
///                            detail::make_directed_container).
/// @tparam Direction          The direction policy: @ref ForwardDirection (the default) or
///                            @ref BackwardDirection.
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>,
          typename Direction = ForwardDirection>
    requires ResourceTypeConcept<ResourceType>
class DominanceAlgorithm : public Algorithm<ResourceType, LabelContainerType> {
        static_assert(DirectionPolicy<Direction, ResourceType>,
                      "Direction does not satisfy DirectionPolicy for this ResourceType");

    public:
        /// @brief The per-node container this search keeps: @c LabelContainerType rebound to
        ///        @c Direction.
        using DirectedContainer = detail::directed_container_t<LabelContainerType, Direction>;

        DominanceAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                           AlgorithmParams<LabelContainerType> params)
            : Algorithm<ResourceType, LabelContainerType>(resource_factory, std::move(params)) {}

        [[nodiscard]] std::vector<SearchDirection> supported_directions() const override {
            return {Direction::backward ? SearchDirection::Backward : SearchDirection::Forward};
        }

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
                non_dominated_labels_by_node_pos_.emplace_back(
                    detail::make_directed_container<Direction>(this->params_.labels));
            }

            for (auto start_node_id : Direction::start_nodes(*this->graph_)) {
                auto* start_node = this->graph_->get_node(start_node_id);
                auto& label = this->label_pool_.get_next_label(start_node);
                // Only start labels need starting values; extension overwrites all others.
                Direction::set_start(*this->graph_, start_node, label);

                auto& buckets = non_dominated_labels_by_node_pos_.at(start_node->pos());
                // it points to the newly inserted element
                auto label_it = buckets.add_label(&label);
                add_new_unprocessed_label(std::make_pair(&label, label_it));
            }
        }

        void main_loop() override {  // NOLINT
            size_t i = 0;
            while (this->waiting_labels() > 0 && !this->should_stop(i)) {
                if (this->check_memory(i)) {
                    break;
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
                if (this->params_.prune_by_incumbent && label.get_cost() >= this->incumbent_) {
                    remove_label(label_iterator_pair.second);
                    // Use release_with_ref_count (not release_label): this label was added to
                    // the non-dominated set, so it pins a predecessor whose ref_count must be
                    // decremented. Plain release_label would leak that predecessor.
                    this->label_pool_.release_with_ref_count(&label);
                    continue;
                }

                assert(label.get_end_node());

                // check if we can update the best label or extend
                if (Direction::is_end_node(label.get_end_node())) {
                    if (label.get_cost() < this->upper_bound_) {
                        if (label.get_cost() < this->incumbent_) {
                            this->incumbent_ = label.get_cost();
                        }
                        if (this->params_.return_dominated_solutions) {
                            this->record_solution(label);
                            if (this->solutions_.size() >= this->params_.max_solutions) {
                                LOG_DEBUG("Stopping after ",
                                          this->solutions_.size(),
                                          " solutions.\n");
                                return;
                            }
                        }
                    }
                } else if (!std::isinf(label.get_cost())) {
                    this->extend(&label);
                } else {
                    remove_label(label_iterator_pair.second);
                    this->label_pool_.release_with_ref_count(&label);
                }
            }
        }

        /// @brief The next label to process and its position in its node's container; a null
        ///        label when none is left. Each subclass decides the order.
        virtual LabelIteratorPair<ResourceType> next_label_iterator() = 0;

        /// @brief Extends @p label_ptr along every arc its direction walks from its node.
        virtual void extend(Label<ResourceType>* label_ptr) {
            const auto& current_node = label_ptr->get_end_node();
            for (auto arc_ptr : Direction::out_arcs(*this->graph_, current_node)) {
                extend_label(label_ptr, arc_ptr);
            }
        }

        /// @brief Extends @p label_ptr along @p arc_ptr into this search's own containers.
        virtual void extend_label(Label<ResourceType>* label_ptr,
                                  const Arc<ResourceType>* arc_ptr) {
            extend_label<Direction>(label_ptr, arc_ptr, non_dominated_labels_by_node_pos_);
        }

        /// @brief Extends @p label_ptr along @p arc_ptr into an explicit container set.
        ///
        /// @tparam LabelDirection The direction to extend in.
        /// @tparam Container      The per-node non-dominated label container type.
        /// @param label_ptr  The label to extend.
        /// @param arc_ptr    The arc to extend along.
        /// @param containers The per-node containers to insert the result into.
        template <typename LabelDirection, typename Container>
        void extend_label(Label<ResourceType>* label_ptr, const Arc<ResourceType>* arc_ptr,
                          std::vector<Container>& containers) {
            auto* head_node = LabelDirection::head(*arc_ptr);

            // Skip the arc when the label can no longer reach the node it lands on. Both
            // directions call the same `is_reachable`, which is correct only when it depends on
            // the node id alone. A resource whose check reads the label's visited set must declare
            // `JoinRule::Unspecified` (as `ReachableFeasibilityFunction` does), so the pre-solve
            // checks refuse a bidirectional search on it.
            if (!label_ptr->is_reachable(head_node->id)) {
                return;
            }

            // Sources and sinks stay out of a path's interior: the graph refuses an arc that
            // enters a source or leaves a sink (Graph::add_arc), so no extension lands on a node
            // this direction starts from.
            assert(!LabelDirection::is_start_node(head_node) &&
                   "an arc enters a source or leaves a sink");

            auto& new_label = this->label_pool_.get_next_label(head_node);
            LabelDirection::extend(*label_ptr, *arc_ptr, &new_label);

            if (++this->num_extended_labels_ % 100000 == 0) {  // NOLINT
                print_labels();
                LOG_DEBUG("Processed ", this->num_extended_labels_, " labels so far...\n");
            }

            bool feasible = LabelDirection::feasible(new_label);
            if (feasible &&
                dispatch_update_non_dominated_labels<LabelDirection>(new_label, containers)) {
                // Keep the label only if it is feasible and not dominated: store it at its node
                // and queue it for extension.
                auto new_label_it =
                    containers.at(new_label.get_end_node()->pos()).add_label(&new_label);
                add_new_unprocessed_label(std::make_pair(&new_label, new_label_it));
                // Pin predecessor: keep it alive until this label is released.
                new_label.set_prev_label(label_ptr);
            } else {
                // new_label was never a predecessor; release immediately.
                this->label_pool_.release_label(&new_label);
            }
        }

        std::vector<size_t> get_path_arc_ids(const Label<ResourceType>& label) override {
            return path_arc_ids<Direction>(label);
        }

        /// @brief The arc ids of the path @p label ends, in forward order.
        ///
        /// Reconstructed by following prev_label pointers, in O(hops): every accepted label
        /// stores a pointer to its predecessor, kept alive via @ref ref_count until this label is
        /// released. Forward, the in-arc chain is reversed; backward, the out-arc chain is already
        /// in forward order.
        ///
        /// @tparam LabelDirection The direction @p label was extended in.
        template <typename LabelDirection>
        [[nodiscard]] std::vector<size_t> path_arc_ids(const Label<ResourceType>& label) const {
            if constexpr (LabelDirection::backward) {
                std::vector<size_t> arc_ids;
                append_suffix_arc_ids(label, &arc_ids);
                return arc_ids;
            } else {
                return prefix_arc_ids(label);
            }
        }

        void record_solution(const Label<ResourceType>& label) override {
            extract_path_solution<Direction>(label);
        }

        /// @brief Records the path @p label ends.
        ///
        /// Going backward, a label ends at a source, so the path's last node is the head of its
        /// last arc: the sink the chain started from.
        ///
        /// @tparam LabelDirection The direction @p label was extended in.
        template <typename LabelDirection>
        void extract_path_solution(const Label<ResourceType>& label) {
            if constexpr (LabelDirection::backward) {
                auto arc_ids = path_arc_ids<LabelDirection>(label);
                if (arc_ids.empty()) {
                    return;
                }
                const size_t end_node_id = this->graph_->get_arc(arc_ids.back())->destination->id;
                this->record_path(label.get_cost(), std::move(arc_ids), end_node_id);
            } else {
                Algorithm<ResourceType, LabelContainerType>::record_solution(label);
            }
        }

        /// @brief Updates @p containers with @p label, through the virtual overload when they are
        ///        this search's own set, so a subclass override runs.
        ///
        /// The backward set of a bidirectional search has another container type, so a forward
        /// override could not apply to it anyway.
        template <typename LabelDirection, typename Container>
        bool dispatch_update_non_dominated_labels(const Label<ResourceType>& label,
                                                  std::vector<Container>& containers) {
            if constexpr (std::is_same_v<LabelDirection, Direction> &&
                          std::is_same_v<Container, DirectedContainer>) {
                if (&containers == &non_dominated_labels_by_node_pos_) {
                    return update_non_dominated_labels(label);
                }
            }
            return update_non_dominated_labels<LabelDirection>(label, containers);
        }

        /// @brief Updates this search's own non-dominated set with @p label.
        virtual bool update_non_dominated_labels(const Label<ResourceType>& label) {
            return update_non_dominated_labels<Direction>(label, non_dominated_labels_by_node_pos_);
        }

        /// @brief Updates an explicit non-dominated set with @p label.
        ///
        /// @tparam LabelDirection The direction whose dominance order applies.
        /// @tparam Container      The per-node non-dominated label container type.
        /// @param label      The candidate label.
        /// @param containers The per-node containers to test against and prune.
        /// @return @c true when @p label is not dominated and should be kept.
        template <typename LabelDirection, typename Container>
        bool update_non_dominated_labels(const Label<ResourceType>& label,
                                         std::vector<Container>& containers) {
            auto current_node_pos = label.get_end_node()->pos();
            auto& non_dominated_labels_list = containers.at(current_node_pos);

            // First, check if label is dominated by any existing non-dominated label
            bool label_dominated = non_dominated_labels_list.is_dominated(label);
            if (label_dominated) {
                return false;
            }

            // Second, remove all existing labels that are dominated by label
            non_dominated_labels_list.remove_dominated_labels(label);

            return true;
        }

        /// @brief Removes @p label_iterator from this search's own containers.
        virtual void remove_label(const std::list<Label<ResourceType>*>::iterator& label_iterator) {
            remove_label<Direction>(label_iterator, non_dominated_labels_by_node_pos_);
        }

        /// @brief Removes @p label_iterator from an explicit container set.
        ///
        /// @tparam LabelDirection The direction the containers belong to.
        /// @tparam Container      The per-node non-dominated label container type.
        /// @param label_iterator Iterator into the container holding the label.
        /// @param containers The per-node containers to erase from.
        template <typename LabelDirection, typename Container>
        void remove_label(const std::list<Label<ResourceType>*>::iterator& label_iterator,
                          std::vector<Container>& containers) {
            auto current_node_pos = (*label_iterator)->get_end_node()->pos();
            containers.at(current_node_pos).erase_label(label_iterator);
        }

        [[nodiscard]] std::list<Label<ResourceType>*> labels_at_end_nodes() const override {
            return labels_at_end_nodes<Direction>(non_dominated_labels_by_node_pos_);
        }

        /// @brief Collects the labels at this direction's terminal nodes.
        ///
        /// @tparam LabelDirection The direction whose terminals to sweep.
        /// @tparam Container      The per-node non-dominated label container type.
        /// @param containers The per-node containers to read.
        /// @return Every label held at a terminal node of @c LabelDirection.
        template <typename LabelDirection, typename Container>
        [[nodiscard]] std::list<Label<ResourceType>*> labels_at_end_nodes(
            const std::vector<Container>& containers) const {
            std::list<Label<ResourceType>*> labels_at_terminals;
            for (auto terminal_node_id : LabelDirection::end_nodes(*this->graph_)) {
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

        /// @brief Queues a newly stored label for extension, in the subclass's own order.
        virtual void add_new_unprocessed_label(
            const LabelIteratorPair<ResourceType>& label_iterator_pair) = 0;

        /// @brief This search's non-dominated labels: one container per node, indexed by node
        ///        position.
        std::vector<DirectedContainer> non_dominated_labels_by_node_pos_;
};
}  // namespace rcspp
