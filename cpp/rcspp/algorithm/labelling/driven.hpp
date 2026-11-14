// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <utility>

#include "rcspp/algorithm/algorithm_params.hpp"
#include "rcspp/algorithm/arc_mask.hpp"
#include "rcspp/algorithm/labelling/parts/driver/driver.hpp"
#include "rcspp/algorithm/limits.hpp"
#include "rcspp/algorithm/proof_record.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp::detail {

/// @brief A labelling search run several times in one solve by @p DriverType
///        (@c parts::Driver), which changes it between runs.
///
/// A run is one call of @ref main_loop. It takes labels until a label it records ends it
/// (@c end_runs_at_end_node, or @c max_solutions reached), a stop applies (the time limit, an
/// interrupt, the memory limit), or no label is left. A run that runs out of labels while the
/// per-node quota or memory pressure stored some aside goes on with them, in up to
/// @c max_phases phases, as a search that is not driven does (@ref main_loop). After each run,
/// the solve's loop (@c Algorithm::solve) records the labels left at the end nodes, then calls
/// @ref prepare_next_run, which asks the driver whether to run again. @c max_iterations does not
/// count a run's labels: a driver counts its runs against it, if it reads it. The members the
/// driver calls are public.
///
/// @tparam Base       The search: a @c LabellingAlgorithm.
/// @tparam DriverType The driver.
template <typename Base, typename DriverType>
class Driven : public Base {
    public:
        using resource_type = typename Base::search_type::resource_type;

        template <typename ResourceFactoryType, typename ParamsType>
        Driven(ResourceFactoryType* resource_factory, ParamsType params)
            : Base(resource_factory, std::move(params)), driver_(this->params_) {
            static_assert(parts::Driver<DriverType, Driven>,
                          "the strategy's driver does not satisfy rcspp::parts::Driver: it needs a "
                          "constructor from the params, begin(search) and after_run(search, "
                          "number_of_runs)");
        }

        /// @brief The graph being solved.
        [[nodiscard]] const Graph<resource_type>& graph() const { return *this->graph_; }

        /// @brief The algorithm's params.
        [[nodiscard]] const AlgorithmBaseParams& params() const { return this->params_; }

        /// @brief The solve's cost upper bound.
        [[nodiscard]] double upper_bound() const { return this->upper_bound_; }

        /// @brief How many solutions the solve has recorded.
        [[nodiscard]] size_t number_of_solutions() const { return this->solutions_.size(); }

        /// @brief How the last run ended.
        [[nodiscard]] const parts::RunEnd& last_run() const { return run_; }

        /// @brief Makes the runs of this solve read @p mask instead of the one @c set_arc_mask
        ///        set (@c get_arc_mask), which they then do not read at all.
        ///
        /// Call it from the driver's @c begin: the search's context, which holds the mask, is
        /// built afresh at the start of each solve. The driver owns @p mask, and may change it
        /// between runs. A driver that must keep the hard arcs of the mask set copies their
        /// states first, as @c detail::TabuArcs::begin does. Unlike @c set_arc_mask, this
        /// records no loss of proof: a driver whose mask may hide a path calls @ref lose_proof.
        void use_arc_mask(const ArcMask* mask) { this->context_.arc_mask = mask; }

        /// @brief Has each run end once it records an end-node label, whatever the retention:
        ///        the take rule records the label when it is taken.
        void end_runs_at_end_node() { this->context_.run_ends_at_end_node = true; }

        /// @brief Records that the solve's result is not a proof, for @p loss.
        void lose_proof(ProofLoss loss) { this->proof_.record(loss); }

        /// @brief Readies the next run from the start labels: the containers and the schedule
        ///        are emptied, the pool takes back every label of the previous run (those still
        ///        waiting too), and the start labels are created again.
        ///
        /// They wait until the next run takes them: a driver that reseeds and then stops leaves
        /// them waiting, so the solve ends @c Truncated, not @c Complete.
        void reseed() {
            this->restart_search();
            this->initialize_labels();
        }

    protected:
        void initialize(const Graph<resource_type>* graph, double cost_upper_bound) override {
            Base::initialize(graph, cost_upper_bound);
            run_ = {};
            refused_ = !driver_.begin(*this);
        }

        /// @brief One run, unless the driver refused to run.
        ///
        /// A run that runs out of labels to take while the per-node quota or memory pressure
        /// stored some aside goes on with them, in up to @c max_phases phases, as a search that
        /// is not driven does. Past the last phase, the labels still stored aside are put back to
        /// wait, and the run @c ended_with_stored_aside, not @c exhausted.
        void main_loop() override {
            run_ = {};
            if (refused_) {
                return;
            }
            for (size_t phase = 1;; ++phase) {
                // A label it recorded ended the run (end_runs_at_end_node, max_solutions), or a
                // stop (time limit, interrupt, memory limit) left labels waiting: the run is over.
                if (this->run_loop(kNoLimit) || this->number_of_waiting_labels() > 0) {
                    break;
                }
                // Out of labels to take: put back what was stored aside for the next phase, if
                // any.
                this->prepare_next_phase();
                if (this->number_of_waiting_labels() == 0) {
                    run_.exhausted = true;
                    break;
                }
                if (phase >= this->params_.max_phases) {
                    run_.ended_with_stored_aside = true;
                    break;
                }
            }
        }

        bool prepare_next_run(size_t number_of_runs) override {
            return !refused_ && driver_.after_run(*this, number_of_runs);
        }

        /// @brief Records @p label as the base does, and notes it in the run's end.
        void record_solution(const Label<resource_type>& label) override {
            const size_t before = this->solutions_.size();
            run_.recorded = true;
            run_.cost = label.get_cost();
            run_.arc_ids = this->get_path_arc_ids(label);
            Base::record_solution(label);
            run_.new_solution = run_.new_solution || this->solutions_.size() > before;
        }

    private:
        DriverType driver_;
        parts::RunEnd run_;
        bool refused_ = false;
};

}  // namespace rcspp::detail
