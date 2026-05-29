// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <list>
#include <utility>

#include "rcspp/algorithm/backtracking_dive_algorithm.hpp"
#include "rcspp/algorithm/tabu_list.hpp"

namespace rcspp {

/**
 * @brief TabuSearchAlgorithm for Resource Constrained Shortest Path Problems (RCSPP).
 *
 * Self-contained tabu search built in the same spirit as @ref GreedyAlgorithm: each
 * iteration performs a greedy dive from a source to a sink, sorting siblings by cost and
 * backtracking on dead-ends. The difference is that arcs used in previously discovered
 * solutions are added to a tabu list with a tenure (in iterations) and are skipped during
 * extension while their tenure remains positive. An aspiration rule allows a tabu arc to
 * be used if no non-tabu extension is feasible from the current label, so the search can
 * always make progress.
 *
 * Compared to @ref DiversificationSearch (which wraps another algorithm and physically
 * removes arcs from a cloned graph), TabuSearchAlgorithm is a single-pass labeller with
 * the lighter overhead of a tabu lookup at extension time. It shares all DFS plumbing
 * with @ref GreedyAlgorithm via @ref BacktrackingDiveAlgorithm and only customises:
 *   - @ref select_children to filter on the tabu list (with aspiration), and
 *   - @ref main_loop to drive episodic dives, sink extraction, tabu update and reset.
 *
 * Use this when you want a fast diversifying constructor that produces multiple distinct
 * feasible paths quickly (e.g. to seed a tighter UB for a dominance run, or to feed a
 * column generator), without the cost of repeatedly cloning and mutating the graph.
 */
template <typename ResourceType, typename LabelsType = LabelList<ResourceType>>
class TabuSearchAlgorithm : public BacktrackingDiveAlgorithm<ResourceType, LabelsType> {
    public:
        TabuSearchAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                            AlgorithmParams<LabelsType> params)
            : BacktrackingDiveAlgorithm<ResourceType, LabelsType>(resource_factory,
                                                                  std::move(params)),
              tabu_(this->params_.seed) {}

    protected:
        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Algorithm<ResourceType, LabelsType>::initialize(graph, cost_upper_bound);
            tabu_.clear();
        }

        // ------------------------------------------------------------------
        // child selection: tabu filter + aspiration
        // ------------------------------------------------------------------

        void select_children(Label<ResourceType>* parent, std::list<Label<ResourceType>*>& feasible,
                             std::list<Label<ResourceType>*>& rejects) override {
            std::list<Label<ResourceType>*> non_tabu;
            std::list<Label<ResourceType>*> tabu_only;
            for (auto* l : feasible) {
                const auto* in_arc = l->get_in_arc();
                if (in_arc != nullptr && tabu_.is_tabu(in_arc->id)) {
                    tabu_only.push_back(l);
                } else {
                    non_tabu.push_back(l);
                }
            }

            std::list<Label<ResourceType>*> chosen;
            if (!non_tabu.empty()) {
                // discard tabu candidates
                rejects.splice(rejects.end(), tabu_only);
                chosen = std::move(non_tabu);
            } else {
                // aspiration: nothing else available, fall back to tabu set
                chosen = std::move(tabu_only);
            }

            (void)parent;
            chosen.sort([](Label<ResourceType>* a, Label<ResourceType>* b) {
                return a->get_cost() < b->get_cost();
            });
            feasible = std::move(chosen);
        }

        // ------------------------------------------------------------------
        // main loop: episodic dives
        // ------------------------------------------------------------------

        void main_loop() override {
            if (this->params_.max_iterations >= MAX_INT) {
                LOG_ERROR(
                    "max_iterations must be set to a finite value for TabuSearchAlgorithm.\n");
                return;
            }

            size_t i = 0;
            while (!this->should_stop(i)) {
                ++i;

                bool reached_sink = dive_to_sink();
                if (reached_sink) {
                    auto* sink_label = this->path_.back().first;
                    bool added = false;
                    if (sink_label->get_cost() < this->cost_upper_bound_) {
                        if (sink_label->get_cost() + this->params_.tolerance <
                            this->best_cost_upper_bound_) {
                            this->best_cost_upper_bound_ = sink_label->get_cost();
                            LOG_DEBUG("Found a better solution with cost ",
                                      sink_label->get_cost(),
                                      "\n");
                        }
                        size_t before = this->solutions_.size();
                        this->extract_solution(*sink_label);
                        added = (this->solutions_.size() > before);
                    }
                    apply_tabu(*sink_label, added);
                } else {
                    if (tabu_.empty()) {
                        // truly no feasible path exists
                        LOG_DEBUG("TabuSearchAlgorithm: no feasible path, stopping.\n");
                        this->clear_path();
                        return;
                    }
                    tabu_.grow_extra();
                }

                tabu_.age();
                this->clear_path();
                this->seed_path_from_sources();
            }

            LOG_DEBUG("TabuSearchAlgorithm: WHILE nb iter: ", i, "\n");
        }

    private:
        // ------------------------------------------------------------------
        // dive
        // ------------------------------------------------------------------

        bool dive_to_sink() {
            while (!this->path_.empty()) {
                auto* current = this->path_.back().first;
                if (current->get_end_node()->sink) {
                    return true;
                }
                if (this->extend_label(current)) {
                    continue;
                }
                if (!this->backtrack()) {
                    return false;
                }
            }
            return false;
        }

        // ------------------------------------------------------------------
        // tabu update
        // ------------------------------------------------------------------

        /// Walk the current path_ up to @p sink_label and add each in-arc to the
        /// tabu list (skipping arcs whose endpoints are forbidden). Adapts the
        /// adaptive extra tenure based on whether the iteration produced a new
        /// solution.
        void apply_tabu(const Label<ResourceType>& sink_label, bool added_new_solution) {
            for (const auto& p : this->path_) {
                const auto* in_arc = p.first->get_in_arc();
                if (in_arc == nullptr) {
                    continue;
                }
                if (this->params_.forbidden_tabu.contains(in_arc->origin->id) ||
                    this->params_.forbidden_tabu.contains(in_arc->destination->id)) {
                    continue;
                }
                tabu_.add(in_arc->id, this->params_.tabu_tenure, this->params_.tabu_random_noise);
                if (p.first == &sink_label) {
                    break;
                }
            }

            if (added_new_solution) {
                tabu_.shrink_extra();
            } else {
                tabu_.grow_extra();
            }
        }

        // ------------------------------------------------------------------
        // state
        // ------------------------------------------------------------------

        TabuList tabu_;
};

}  // namespace rcspp
