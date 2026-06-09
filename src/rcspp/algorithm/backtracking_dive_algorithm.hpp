// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <list>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp {

/**
 * @brief Common DFS-with-backtracking machinery shared by the constructive
 * heuristics in this package (e.g. @ref GreedyAlgorithm, @ref TabuSearchAlgorithm).
 *
 * The class owns a @c path_ that represents the current depth-first path under
 * exploration: each entry holds the label currently selected at that depth and
 * an ordered list of remaining sibling labels to try if a dead-end forces a
 * backtrack. Subclasses drive the search through @ref extend_label and
 * @ref backtrack, and they customise selection of the next children via the
 * @ref select_children hook (e.g. cost-sorting only, or tabu filtering).
 *
 * This class does not implement @ref main_loop: each subclass picks how to
 * sequence dives, sink-extractions, and resets.
 */
template <typename ResourceType, typename LabelsType = LabelList<ResourceType>>
class BacktrackingDiveAlgorithm : public Algorithm<ResourceType, LabelsType> {
    public:
        BacktrackingDiveAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                                  AlgorithmParams<LabelsType> params)
            : Algorithm<ResourceType, LabelsType>(resource_factory, std::move(params)) {}

    protected:
        using PathEntry = std::pair<Label<ResourceType>*, std::list<Label<ResourceType>*>>;

        void initialize_labels() override { seed_path_from_sources(); }

        [[nodiscard]] size_t number_of_labels() const override { return path_.empty() ? 0 : 1; }

        [[nodiscard]] std::list<Label<ResourceType>*> get_labels_at_sinks() const override {
            return {};
        }

        std::vector<size_t> get_path_arc_ids(const Label<ResourceType>& label) override {
            std::vector<size_t> path_arc_ids;
            for (const auto& p : path_) {
                const auto* in_arc = p.first->get_in_arc();
                if (in_arc != nullptr) {
                    path_arc_ids.push_back(in_arc->id);
                }
                if (p.first == &label) {
                    break;
                }
            }
            return path_arc_ids;
        }

        // ------------------------------------------------------------------
        // path_ management
        // ------------------------------------------------------------------

        /// Seed @c path_ with one entry per source node.
        /// Subsequent siblings at depth 0 are the source labels other than the first.
        void seed_path_from_sources() {
            std::list<Label<ResourceType>*> sources;
            for (auto src_id : this->graph_->get_source_node_ids()) {
                auto* src = this->graph_->get_node(src_id);
                sources.push_back(&this->label_pool_.get_next_label(src));
            }
            if (sources.empty()) {
                return;
            }
            path_.clear();
            add_labels_to_path(std::move(sources));
        }

        /// Release every label held in @c path_ (current and siblings) and clear it.
        void clear_path() {
            for (auto& entry : path_) {
                this->label_pool_.release_label(entry.first);
                for (auto* sib : entry.second) {
                    this->label_pool_.release_label(sib);
                }
            }
            path_.clear();
        }

        /// Push a new depth onto @c path_: the first label becomes the current, the
        /// rest become its siblings.
        void add_labels_to_path(std::list<Label<ResourceType>*> labels) {
            auto* first = labels.front();
            labels.pop_front();
            path_.emplace_back(first, std::move(labels));
        }

        // ------------------------------------------------------------------
        // dive primitives
        // ------------------------------------------------------------------

        /// Try to extend @p label by one level. Builds all feasible extensions,
        /// asks @ref select_children for the chosen ordered subset (with rejects),
        /// and pushes the chosen list as a new depth in @c path_. Returns true
        /// iff a new depth was pushed.
        bool extend_label(Label<ResourceType>* label) {
            std::list<Label<ResourceType>*> feasible;
            for (auto* arc : label->get_end_node()->out_arcs) {
                if (!label->is_reachable(arc->destination->id)) {
                    continue;
                }
                auto& candidate = this->label_pool_.get_next_label(arc->destination);
                label->extend(*arc, &candidate);
                if (candidate.is_feasible()) {
                    feasible.push_back(&candidate);
                } else {
                    this->label_pool_.release_label(&candidate);
                }
            }

            std::list<Label<ResourceType>*> rejects;
            select_children(label, feasible, rejects);
            for (auto* r : rejects) {
                this->label_pool_.release_label(r);
            }

            if (feasible.empty()) {
                return false;
            }
            add_labels_to_path(std::move(feasible));
            return true;
        }

        /// Pop dead-end depths and switch to the next sibling at the deepest depth
        /// that still has one. Returns false iff @c path_ has been exhausted.
        bool backtrack() {
            while (!path_.empty() && path_.back().second.empty()) {
                this->label_pool_.release_label(path_.back().first);
                path_.pop_back();
            }
            if (path_.empty()) {
                return false;
            }
            this->label_pool_.release_label(path_.back().first);
            auto* next = path_.back().second.front();
            path_.back().second.pop_front();
            path_.back().first = next;
            return true;
        }

        // ------------------------------------------------------------------
        // customisation hook
        // ------------------------------------------------------------------

        /// Choose which feasible extensions of @p parent to keep and in what order.
        ///
        /// On entry, @p feasible contains every feasible extension built by
        /// @ref extend_label. On return, @p feasible holds the ordered list of
        /// labels to push as a new depth (best-first, since the head is selected
        /// as the new "current" and the rest become its siblings), and @p rejects
        /// holds any labels that should be released by the caller.
        ///
        /// The default implementation sorts ascending by label cost and produces
        /// no rejects — i.e. classic greedy extension order.
        virtual void select_children(Label<ResourceType>* parent,
                                     std::list<Label<ResourceType>*>& feasible,
                                     std::list<Label<ResourceType>*>& rejects) {
            (void)parent;
            (void)rejects;
            feasible.sort([](Label<ResourceType>* a, Label<ResourceType>* b) {
                return a->get_cost() < b->get_cost();
            });
        }

        // ------------------------------------------------------------------
        // misc helpers
        // ------------------------------------------------------------------

        [[nodiscard]] std::string to_string() const {
            std::stringstream ss;
            size_t n = path_.size();
            for (const auto& p : path_) {
                ss << p.first->get_end_node()->id << (--n == 0 ? "" : " -> ");
            }
            return ss.str();
        }

        // ------------------------------------------------------------------
        // state
        // ------------------------------------------------------------------

        std::list<PathEntry> path_;
};

}  // namespace rcspp
