// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cassert>
#include <list>
#include <stdexcept>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/label_buckets.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp::detail {

/// @brief The container a search in @p Dir keeps, given the container type @p LC its params hold.
///
/// Forward keeps @p LC. Backward needs a container that compares labels backward; only
/// @ref LabelList has one, so @c rebind_direction<LabelBuckets<...>, BackwardDirection> is left
/// undefined and a backward search with buckets is refused when it is created.
///
/// @tparam LC  The container type of the algorithm's params.
/// @tparam Dir The search's direction policy.
template <typename LC, typename Dir>
struct rebind_direction;

template <typename LC>
struct rebind_direction<LC, ForwardDirection> {
        using type = LC;

        /// @brief An empty container configured as @p prototype.
        static LC convert(const LC& prototype) { return prototype.copy(); }
};

template <typename R, typename D>
struct rebind_direction<LabelList<R, D>, BackwardDirection> {
        using type = LabelList<R, BackwardDirection>;

        /// @brief An empty backward list (a list has no configuration to carry over).
        static type convert(const LabelList<R, D>& /*prototype*/) { return type{}; }
};

template <typename LC, typename Dir>
using rebind_direction_t = typename rebind_direction<LC, Dir>::type;

/// @brief Whether a search in @p Dir can keep its labels in @p LC's rebinding.
template <typename LC, typename Dir>
inline constexpr bool can_rebind_direction_v =
    requires { typename rebind_direction<LC, Dir>::type; };  // NOLINT(whitespace/newline)

/// @brief The labels one direction of a search keeps: one non-dominated container per node, and
///        what is done to them.
///
/// A single-direction search owns one half; a bidirectional search owns two, one per direction,
/// and drives both through the same code. Where an accepted label goes next (a frontier, a queue)
/// is not the half's business: @ref seed and @ref extend_along hand it to a callable.
///
/// @tparam ResourceType The resource type carried by labels.
/// @tparam Dir          The direction policy: @ref ForwardDirection or @ref BackwardDirection.
/// @tparam Container    The per-node non-dominated label container, comparing labels in @p Dir.
template <typename ResourceType, typename Dir, typename Container>
class SearchHalf {
    public:
        using direction = Dir;
        using container_type = Container;

        /// @brief Empties the half and gives it one container per node.
        ///
        /// @param num_nodes The number of node positions.
        /// @param prototype An empty container, configured as the search wants; each node gets
        ///                  its @c copy().
        void reset(size_t num_nodes, const Container& prototype) {
            containers_.clear();
            containers_.reserve(num_nodes);
            for (size_t i = 0; i < num_nodes; ++i) {
                containers_.emplace_back(prototype.copy());
            }
        }

        /// @brief Forgets every container, releasing no label (the pool owns them).
        void clear() { containers_.clear(); }

        /// @brief Installs @p mask on every container.
        ///
        /// @param mask See @c Algorithm::dominance_mask; empty leaves the containers exact.
        /// @throws std::invalid_argument when @p mask is not empty and @p Container cannot leave
        ///         components out of dominance.
        void apply_dominance_mask(const std::vector<bool>& mask) {
            if (mask.empty()) {
                return;
            }
            if constexpr (requires(Container& container) {
                              container.set_ignored_in_dominance(mask);
                          }) {
                for (auto& container : containers_) {
                    container.set_ignored_in_dominance(mask);
                }
            } else {
                throw std::invalid_argument(
                    "dominance_ignored_components needs a label container that can leave "
                    "components out of dominance (LabelList or LabelBuckets)");
            }
        }

        /// @brief Creates the seed labels of this direction, stores them, and hands each to
        ///        @p push.
        ///
        /// Only seed labels need starting values; extension overwrites all others.
        ///
        /// @param graph The graph.
        /// @param pool  The label pool.
        /// @param push  Callable `(const LabelIteratorPair&)` receiving each seed.
        template <typename Push>
        void seed(const Graph<ResourceType>& graph, LabelPool<ResourceType>* pool, Push&& push) {
            for (const size_t seed_node_id : Dir::seeds(graph)) {
                auto* seed_node = graph.get_node(seed_node_id);
                auto& label = pool->get_next_label(seed_node);
                Dir::seed(graph, seed_node, label);
                auto label_it = containers_.at(seed_node->pos()).add_label(&label);
                push(std::make_pair(&label, label_it));
            }
        }

        /// @brief Extends @p label along @p arc into this half.
        ///
        /// A feasible label that @p accept keeps is stored, pinned to @p label, and handed to
        /// @p push; any other is released at once.
        ///
        /// @param label  The label to extend.
        /// @param arc    The arc to extend along.
        /// @param pool   The label pool.
        /// @param accept Callable `(const Label&) -> bool`: whether the extended label is not
        ///               dominated, pruning the labels it dominates (see
        ///               @ref insert_if_non_dominated).
        /// @param push   Callable `(const LabelIteratorPair&)` receiving the accepted label.
        /// @return How the extension ended.
        template <typename Accept, typename Push>
        ExtensionOutcome extend_along(Label<ResourceType>* label, const Arc<ResourceType>* arc,
                                      LabelPool<ResourceType>* pool, Accept&& accept, Push&& push) {
            auto* head_node = Dir::head(*arc);

            // The reachability look-ahead, on the node the extension lands on. Both directions
            // share one @c is_reachable, which is only valid if it is a predicate on the node id;
            // one that reads the label's own collected set must declare @c JoinRule::Unspecified,
            // as @c ReachableFeasibilityFunction does.
            if (!label->is_reachable(head_node->id)) {
                return ExtensionOutcome::Unreachable;
            }

            // Sources and sinks stay out of a path's interior: the graph refuses an arc that
            // enters a source or leaves a sink (Graph::add_arc), so no extension lands on a node
            // this direction starts from.
            assert(!Dir::is_seed(head_node) && "an arc enters a source or leaves a sink");

            auto& new_label = pool->get_next_label(head_node);
            Dir::extend(*label, *arc, &new_label);

            if (!Dir::feasible(new_label)) {
                // new_label was never a predecessor; release immediately.
                pool->release_label(&new_label);
                return ExtensionOutcome::Infeasible;
            }
            if (!accept(new_label)) {
                pool->release_label(&new_label);
                return ExtensionOutcome::Dominated;
            }
            auto new_label_it = containers_.at(head_node->pos()).add_label(&new_label);
            push(std::make_pair(&new_label, new_label_it));
            // Pin predecessor: keep it alive until this label is released.
            new_label.set_prev_label(label);
            return ExtensionOutcome::Accepted;
        }

        /// @brief Whether @p label is not dominated at its node; if so, the labels it dominates
        ///        there are removed (and marked dominated).
        ///
        /// @param label The candidate label, not yet stored.
        /// @return @c true when @p label should be kept.
        bool insert_if_non_dominated(const Label<ResourceType>& label) {
            auto& container = containers_.at(label.get_end_node()->pos());
            if (container.is_dominated(label)) {
                return false;
            }
            container.remove_dominated_labels(label);
            return true;
        }

        /// @brief Removes a stored label from its node's container.
        ///
        /// @param label_iterator Iterator into the container holding the label.
        void erase(const LabelIterator<ResourceType>& label_iterator) {
            containers_.at((*label_iterator)->get_end_node()->pos()).erase_label(label_iterator);
        }

        /// @brief The labels held at this direction's terminal nodes.
        ///
        /// @param graph The graph.
        /// @return Every label at a terminal node of @p Dir.
        [[nodiscard]] std::list<Label<ResourceType>*> labels_at_terminals(
            const Graph<ResourceType>& graph) const {
            std::list<Label<ResourceType>*> labels;
            for (const size_t terminal_node_id : Dir::terminals(graph)) {
                const auto& at_terminal =
                    containers_.at(graph.get_node(terminal_node_id)->pos()).get_labels();
                labels.insert(labels.end(), at_terminal.begin(), at_terminal.end());
            }
            return labels;
        }

        /// @brief The arc ids of the path a label of this direction holds, in path order: its
        ///        prefix going forward (@ref prefix_arc_ids), its suffix going backward
        ///        (@ref append_suffix_arc_ids).
        ///
        /// @param label A label extended in @p Dir.
        /// @return The arc ids.
        [[nodiscard]] static std::vector<size_t> path_arc_ids(const Label<ResourceType>& label) {
            if constexpr (Dir::backward) {
                std::vector<size_t> arc_ids;
                append_suffix_arc_ids(label, &arc_ids);
                return arc_ids;
            } else {
                return prefix_arc_ids(label);
            }
        }

        /// @brief Adds this half's surviving labels and dominance comparisons to the counts.
        ///
        /// @param labels Accumulator for surviving labels; never null.
        /// @param checks Accumulator for dominance comparisons; never null.
        void accumulate_stats(size_t* labels, size_t* checks) const {
            for (const auto& container : containers_) {
                *labels += container.get_labels().size();
                *checks += container.dominance_checks();
            }
        }

        /// @brief Logs every container at TRACE level.
        ///
        /// @param graph The graph, to name the nodes.
        void print(const Graph<ResourceType>& graph) const {
            if (LOG_TRACE_ACTIVE()) {
                LOG_TRACE("All non dominated labels by node:\n");
                for (size_t pos = 0; pos < containers_.size(); pos++) {
                    LOG_TRACE("Node ", graph.get_sorted_nodes().at(pos)->id, ":\n");
                    containers_.at(pos).print_labels();
                }
            }
        }

        /// @brief The per-node containers, by node position.
        [[nodiscard]] std::vector<Container>& containers() { return containers_; }

        /// @brief The per-node containers, by node position.
        [[nodiscard]] const std::vector<Container>& containers() const { return containers_; }

    private:
        std::vector<Container> containers_;
};

/// @brief The labeling loop, written once against a direction policy.
///
/// Its labels live in one @ref SearchHalf; the order they are extended in is the subclass's,
/// through @ref add_new_unprocessed_label and @ref next_label_iterator.
///
/// Internal: the public algorithms derive from it through @ref DominanceAlgorithm.
///
/// @tparam ResourceType       The resource type carried by labels.
/// @tparam LabelContainerType The per-node non-dominated label container its params hold; a
///                            backward search keeps it rebound (@ref rebind_direction).
/// @tparam Dir                The direction policy: @ref ForwardDirection or
///                            @ref BackwardDirection.
template <typename ResourceType, typename LabelContainerType, typename Dir>
    requires ResourceTypeConcept<ResourceType>
class DirectionalDominanceAlgorithm : public Algorithm<ResourceType, LabelContainerType> {
        static_assert(DirectionPolicy<Dir, ResourceType>,
                      "Dir does not satisfy DirectionPolicy for this ResourceType");

    public:
        using resource_type = ResourceType;
        using label_container_type = LabelContainerType;

        /// @brief The per-node container this search keeps: @c LabelContainerType rebound to
        ///        @c Dir.
        using DirectedContainer = rebind_direction_t<LabelContainerType, Dir>;

        DirectionalDominanceAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                                      AlgorithmParams<LabelContainerType> params)
            : Algorithm<ResourceType, LabelContainerType>(resource_factory, std::move(params)) {}

        [[nodiscard]] std::vector<SearchDirection> supported_directions() const override {
            return {Dir::backward ? SearchDirection::Backward : SearchDirection::Forward};
        }

    protected:
        /// @brief Release label memory and clear the non-dominated label containers.
        ///
        /// Overrides @ref Algorithm::release_label_memory() to also clear the containers so that
        /// no dangling label pointers remain after the pool is freed.  Subclasses that own
        /// additional label containers (e.g. unprocessed queues) should override this further and
        /// call this base implementation.
        void release_label_memory() override {
            Algorithm<ResourceType, LabelContainerType>::release_label_memory();
            half_.clear();
        }

        /// @brief Adds this search's surviving-label and dominance-comparison counts.
        ///
        /// Which field the labels land in follows the direction policy, so a backward-only
        /// instantiation reports backward labels rather than mislabelling them as forward.
        ///
        /// Runs before `release_label_memory()` clears the containers; see @c Algorithm::solve,
        /// which orders the two deliberately.
        ///
        /// @param result The result about to be returned; never null.
        void annotate(SolveResult* result) const override {
            Algorithm<ResourceType, LabelContainerType>::annotate(result);
            half_.accumulate_stats(
                Dir::backward ? &result->backward_labels : &result->forward_labels,
                &result->dominance_checks);
        }

        void initialize_labels() override {
            // Release all labels from the previous run (including any pending_release ones)
            // so the pool is fully reset before we start fresh.
            this->label_pool_.release_all_labels();

            half_.reset(this->graph_->get_number_of_nodes(),
                        rebind_direction<LabelContainerType, Dir>::convert(this->params_.labels));
            half_.apply_dominance_mask(this->dominance_mask());
            half_.seed(*this->graph_,
                       &this->label_pool_,
                       [this](const LabelIteratorPair<ResourceType>& seed) {
                           add_new_unprocessed_label(seed);
                       });
        }

        void main_loop() override {  // NOLINT
            size_t i = 0;
            while (this->number_of_labels() > 0 && !this->should_stop(i)) {
                if (this->memory_says_stop(i)) {
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

        /// @brief Extends @p label_ptr along @p arc_ptr into this search's labels.
        ///
        /// Dominance goes through @ref update_non_dominated_labels and an accepted label through
        /// @ref add_new_unprocessed_label, so a subclass overriding either is honoured.
        virtual void extend_label(Label<ResourceType>* label_ptr,
                                  const Arc<ResourceType>* arc_ptr) {
            this->record_extension(half_.extend_along(
                label_ptr,
                arc_ptr,
                &this->label_pool_,
                [this](const Label<ResourceType>& label) {
                    return update_non_dominated_labels(label);
                },
                [this](const LabelIteratorPair<ResourceType>& accepted) {
                    add_new_unprocessed_label(accepted);
                }));
        }

        std::vector<size_t> get_path_arc_ids(const Label<ResourceType>& label) override {
            return SearchHalf<ResourceType, Dir, DirectedContainer>::path_arc_ids(label);
        }

        using Algorithm<ResourceType, LabelContainerType>::extract_solution;

        /// @brief Records the path @p label ends; going backward, the one it starts.
        void extract_solution(const Label<ResourceType>& label) override {
            if constexpr (Dir::backward) {
                this->extract_suffix_solution(label);
            } else {
                Algorithm<ResourceType, LabelContainerType>::extract_solution(label);
            }
        }

        /// @brief Updates this search's non-dominated set with @p label.
        ///
        /// @return @c true when @p label is not dominated and should be kept.
        virtual bool update_non_dominated_labels(const Label<ResourceType>& label) {
            total_update_non_dom_time_.start();
            ++nb_update_non_dom_iter_;
            const bool kept = half_.insert_if_non_dominated(label);
            total_update_non_dom_time_.stop();
            return kept;
        }

        /// @brief Removes @p label_iterator from this search's containers.
        virtual void remove_label(const LabelIterator<ResourceType>& label_iterator) {
            half_.erase(label_iterator);
        }

        [[nodiscard]] std::list<Label<ResourceType>*> get_labels_at_sinks() const override {
            return half_.labels_at_terminals(*this->graph_);
        }

        void print_labels() const override { half_.print(*this->graph_); }

        virtual void add_new_unprocessed_label(
            const LabelIteratorPair<ResourceType>& label_iterator_pair) = 0;

        /// @brief This search's per-node containers, by node position.
        [[nodiscard]] const std::vector<DirectedContainer>& non_dominated_labels() const {
            return half_.containers();
        }

        SearchHalf<ResourceType, Dir, DirectedContainer> half_;

        Timer total_extend_time_;
        Timer total_update_non_dom_time_;

        size_t nb_update_non_dom_iter_ = 0;
        size_t nb_extend_iter_ = 0;
};

}  // namespace rcspp::detail
