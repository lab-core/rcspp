// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <limits>
#include <list>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

namespace test_util {

/// @brief A backward-only labeling algorithm, for testing the backward machinery in isolation.
///
/// Seeds at the sinks, walks in-arcs, extends backward, tests backward feasibility, prunes with
/// backward dominance. Runs to completion -- there is no half-way bound here, and no join.
///
/// Test-only: the production frontier belongs to the bidirectional algorithm. This exists so a
/// wrong extension sign, a wrong seed or an un-routed container shows up on a real graph before
/// the join logic is written, rather than four phases later entangled with it.
///
/// The frontier below is `SimpleDominanceAlgorithm`'s, copied verbatim: a flat FIFO list, a
/// truncation list and a per-node extension counter. Nothing in it is direction-specific, which is
/// the point -- if something here had to change to run backward, that would be a gap in the
/// direction policy rather than something to paper over here.
///
/// @tparam ResourceType       The resource type carried by labels.
/// @tparam LabelContainerType The per-node container; defaults to a *backward* label list, since a
///                            forward-comparing container would prune the better labels.
template <typename ResourceType,
          typename LabelContainerType = rcspp::LabelList<ResourceType, rcspp::BackwardDirection>>
    requires rcspp::ResourceTypeConcept<ResourceType>
class BackwardOnlyAlgorithm
    : public rcspp::DirectionalDominanceAlgorithm<ResourceType, LabelContainerType,
                                                  rcspp::BackwardDirection> {
        using Base = rcspp::DirectionalDominanceAlgorithm<ResourceType, LabelContainerType,
                                                          rcspp::BackwardDirection>;

    public:
        BackwardOnlyAlgorithm(rcspp::ResourceFactory<ResourceType>* resource_factory,
                              rcspp::AlgorithmParams<LabelContainerType> params)
            : Base(resource_factory, std::move(params)) {}

        ~BackwardOnlyAlgorithm() override = default;

        /// @brief Read-only access to the per-node backward label sets, for assertions on values.
        ///
        /// A wrong seed and a wrong reconstruction both present as "no solutions"; asserting on
        /// label values rather than on a solve result is what tells them apart.
        [[nodiscard]] const std::vector<LabelContainerType>& get_labels_by_node_pos() const {
            return this->non_dominated_labels_by_node_pos_;
        }

        /// @brief The labels sitting at this search's terminal nodes -- the graph's *sources*.
        ///
        /// Named "terminals" rather than "sinks" because the inherited hook is
        /// `get_labels_at_sinks()`, which for a backward search sweeps
        /// `BackwardDirection::terminals` and therefore returns labels at sources. If this comes
        /// back empty, suspect that before concluding the search failed.
        [[nodiscard]] std::list<rcspp::Label<ResourceType>*> get_terminal_labels() const {
            return this->get_labels_at_sinks();
        }

        /// @brief The cheapest complete backward path, read from the terminal labels.
        ///
        /// Read from label values rather than from solve()'s solutions, because path
        /// reconstruction for the backward direction does not exist yet: a backward label carries
        /// an out-arc and no in-arc, so `get_path_arc_ids` walks an empty chain and
        /// `extract_solution` drops it. Nothing here depends on that being fixed.
        ///
        /// @return The minimum cost over terminal labels, or infinity when there are none.
        [[nodiscard]] double best_terminal_cost() const {
            double best = std::numeric_limits<double>::infinity();
            for (const auto* label : get_terminal_labels()) {
                best = std::min(best, label->get_cost());
            }
            return best;
        }

    private:
        void initialize(const rcspp::Graph<ResourceType>* graph, double cost_upper_bound) override {
            Base::initialize(graph, cost_upper_bound);
            number_of_extended_labels_per_node_.resize(graph->get_number_of_nodes());
        }

        rcspp::LabelIteratorPair<ResourceType> next_label_iterator() override {
            rcspp::LabelIteratorPair<ResourceType> label_iterator_pair;
            while (!unprocessed_labels_.empty()) {
                label_iterator_pair = unprocessed_labels_.front();
                unprocessed_labels_.pop_front();

                if (label_iterator_pair.first->dominated) {
                    this->label_pool_.release_with_ref_count(label_iterator_pair.first);
                } else {
                    size_t& num_extended_labels_for_node = number_of_extended_labels_per_node_.at(
                        label_iterator_pair.first->get_end_node()->pos());
                    if (num_extended_labels_for_node < this->effective_max_labels_per_node_) {
                        ++num_extended_labels_for_node;
                        break;
                    }
                    unprocessed_truncated_labels_.push_back(label_iterator_pair);
                }
            }

            return label_iterator_pair;
        }

        [[nodiscard]] size_t number_of_labels() const override {
            return unprocessed_labels_.size();
        }

        void add_new_unprocessed_label(
            const rcspp::LabelIteratorPair<ResourceType>& label_iterator_pair) override {
            unprocessed_labels_.push_back(label_iterator_pair);
        }

        void prepareNextPhase() override {
            std::ranges::fill(number_of_extended_labels_per_node_.begin(),
                              number_of_extended_labels_per_node_.end(),
                              0);
            unprocessed_labels_.splice(unprocessed_labels_.end(), unprocessed_truncated_labels_);
        }

        void release_label_memory() override {
            Base::release_label_memory();
            unprocessed_labels_.clear();
            unprocessed_truncated_labels_.clear();
            std::ranges::fill(number_of_extended_labels_per_node_.begin(),
                              number_of_extended_labels_per_node_.end(),
                              0);
        }

        std::list<rcspp::LabelIteratorPair<ResourceType>> unprocessed_labels_;
        std::list<rcspp::LabelIteratorPair<ResourceType>> unprocessed_truncated_labels_;
        std::vector<size_t> number_of_extended_labels_per_node_;
};

}  // namespace test_util
