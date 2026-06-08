// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <list>
#include <utility>
#include <vector>

#include "rcspp/algorithm/backtracking_dive_algorithm.hpp"
#include "rcspp/algorithm/tabu_list.hpp"

namespace rcspp {

/// @brief Classical improving tabu search for RCSPP.
///
/// Two-phase algorithm built on the same DFS-with-backtracking engine as
/// @ref GreedyAlgorithm and @ref TabuSearchAlgorithm:
///
/// **Phase 1 — construction**: a pure greedy dive (no tabu) finds an initial
/// feasible solution and records its cost as the best known upper bound.
///
/// **Phase 2 — improvement**: repeated tabu-filtered dives from the source,
/// each one constrained to find a path strictly cheaper than the current best.
/// Classical tabu-search mechanisms are applied:
///   - **Tabu list**: arcs used in the most recently found path are forbidden
///     for @c tabu_tenure iterations, preventing cycling.
///   - **Aspiration criterion**: when every extension from a node is tabu, the
///     cheapest tabu extension is used anyway (last-resort aspiration).
///   - **Intensification**: when a strictly improving solution is found, the
///     adaptive tenure is shrunk so the search stays near the good region.
///   - **Diversification**: when @c diversification_tenure consecutive dives
///     fail to improve, the adaptive tenure is grown to force exploration of
///     unexplored regions; the counter then resets.
///
/// All solutions found in both phases are collected.  The algorithm is not
/// optimal; it is intended as a fast primal heuristic.
///
/// @tparam ResourceType      Composed resource type (must satisfy ResourceTypeConcept).
/// @tparam LabelContainerType Non-dominated label container (default: LabelList).
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>>
    requires ResourceTypeConcept<ResourceType>
class ImprovingTabuSearch : public BacktrackingDiveAlgorithm<ResourceType, LabelContainerType> {
    public:
        /// @brief Construct with a resource factory and algorithm parameters.
        ///
        /// @param resource_factory Factory for source-node label initialisation.
        /// @param params           Algorithm configuration.  Relevant fields:
        ///   - @c max_iterations       – improvement-phase iteration budget.
        ///   - @c tabu_tenure          – base tabu tenure for arcs.
        ///   - @c tabu_random_noise    – whether to add jitter to tenure.
        ///   - @c stop_after_X_solutions – early-stop threshold.
        ///   - @c diversification_tenure – no-improve count before grow_extra().
        ImprovingTabuSearch(ResourceFactory<ResourceType>* resource_factory,
                            AlgorithmParams<LabelContainerType> params)
            : BacktrackingDiveAlgorithm<ResourceType, LabelContainerType>(resource_factory,
                                                                          std::move(params)),
              tabu_(this->params_.seed) {}

        ~ImprovingTabuSearch() override = default;

        [[nodiscard]] bool is_optimal() const override { return false; }

    protected:
        // ─── child selection ─────────────────────────────────────────────────

        /// @brief Select children: filter tabu arcs, fall back to them on aspiration.
        ///
        /// Non-tabu extensions are kept in @p feasible (sorted ascending by cost).
        /// If all extensions are tabu, the cheapest one is kept (aspiration criterion).
        void select_children(Label<ResourceType>* parent, std::list<Label<ResourceType>*>& feasible,
                             std::list<Label<ResourceType>*>& rejects) override {
            if (!tabu_active_) {
                // Phase 1: pure greedy — no filtering.
                (void)parent;
                feasible.sort([](Label<ResourceType>* a, Label<ResourceType>* b) {
                    return a->get_cost() < b->get_cost();
                });
                return;
            }

            // Partition into non-tabu and tabu extensions.
            std::list<Label<ResourceType>*> non_tabu;
            std::list<Label<ResourceType>*> tabu_only;
            for (auto* l : feasible) {
                const auto* arc = l->get_in_arc();
                if (arc != nullptr && tabu_.is_tabu(arc->id)) {
                    tabu_only.push_back(l);
                } else {
                    non_tabu.push_back(l);
                }
            }

            std::list<Label<ResourceType>*>* chosen = nullptr;
            std::list<Label<ResourceType>*>* discarded = nullptr;
            if (!non_tabu.empty()) {
                chosen = &non_tabu;
                discarded = &tabu_only;
            } else {
                // Aspiration: nothing non-tabu — use cheapest tabu extension.
                chosen = &tabu_only;
                discarded = &non_tabu;
            }

            (void)parent;
            chosen->sort([](Label<ResourceType>* a, Label<ResourceType>* b) {
                return a->get_cost() < b->get_cost();
            });
            rejects.splice(rejects.end(), *discarded);
            feasible = std::move(*chosen);
        }

        // ─── main loop ───────────────────────────────────────────────────────

        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Algorithm<ResourceType, LabelContainerType>::initialize(graph, cost_upper_bound);
            tabu_.clear();
            tabu_active_ = false;
        }

        void main_loop() override {  // NOLINT(readability-function-cognitive-complexity)
            if (this->params_.max_iterations >= MAX_INT) {
                LOG_ERROR(
                    "ImprovingTabuSearch: max_iterations must be finite for the improvement "
                    "phase.\n");
                return;
            }

            // ── Phase 1: greedy construction (tabu inactive) ──────────────────
            this->seed_path_from_sources();
            if (this->path_.empty()) {
                return;
            }

            bool reached = dive_to_sink();
            if (!reached) {
                this->clear_path();
                return;
            }

            auto* init_label = this->path_.back().first;
            double best_cost = init_label->get_cost();
            if (best_cost < this->cost_upper_bound_) {
                this->best_cost_upper_bound_ = best_cost;
                this->extract_solution(*init_label);
                apply_tabu(*init_label);
            }
            this->clear_path();

            if (this->should_stop()) {
                return;
            }

            // ── Phase 2: tabu-filtered improvement ────────────────────────────
            tabu_active_ = true;
            size_t no_improve_count = 0;

            for (size_t i = 0; !this->should_stop(i); ++i) {
                this->seed_path_from_sources();
                if (this->path_.empty()) {
                    break;
                }

                reached = dive_to_sink();

                if (reached) {
                    auto* sink_label = this->path_.back().first;
                    double cost = sink_label->get_cost();

                    if (cost < this->cost_upper_bound_) {
                        this->extract_solution(*sink_label);
                        if (cost + this->params_.tolerance < best_cost) {
                            // Strictly improving solution — intensify.
                            best_cost = cost;
                            this->best_cost_upper_bound_ = best_cost;
                            no_improve_count = 0;
                            tabu_.shrink_extra();
                        } else {
                            // Novel but not strictly improving — count as no-improve.
                            no_improve_count++;
                        }
                    } else {
                        no_improve_count++;
                    }

                    apply_tabu(*sink_label);

                    if (this->solutions_.size() >= this->params_.stop_after_X_solutions) {
                        this->clear_path();
                        break;
                    }
                } else {
                    if (tabu_.empty()) {
                        this->clear_path();
                        break;
                    }
                    tabu_.grow_extra();
                    no_improve_count++;
                }

                // Diversification: too many non-improving iterations.
                if (no_improve_count >= this->params_.diversification_tenure) {
                    tabu_.grow_extra();
                    no_improve_count = 0;
                }

                tabu_.age();
                this->clear_path();
            }
        }

    private:
        // ─── helpers ─────────────────────────────────────────────────────────
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

        void apply_tabu(const Label<ResourceType>& sink_label) {
            for (const auto& entry : this->path_) {
                const auto* arc = entry.first->get_in_arc();
                if (arc == nullptr) {
                    continue;
                }
                if (this->params_.forbidden_tabu.contains(arc->origin->id) ||
                    this->params_.forbidden_tabu.contains(arc->destination->id)) {
                    continue;
                }
                tabu_.add(arc->id, this->params_.tabu_tenure, this->params_.tabu_random_noise);
                if (entry.first == &sink_label) {
                    break;
                }
            }
        }

        // ─── state ───────────────────────────────────────────────────────────
        TabuList tabu_;
        bool tabu_active_ = false;
};

}  // namespace rcspp
