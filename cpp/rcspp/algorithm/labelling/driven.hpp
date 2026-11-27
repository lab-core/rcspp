// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm_params.hpp"
#include "rcspp/algorithm/arc_mask.hpp"
#include "rcspp/algorithm/labelling/parts/driver/driver.hpp"
#include "rcspp/algorithm/limits.hpp"
#include "rcspp/algorithm/proof_record.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp::detail {

/// @brief A labelling search that a driver runs several times in one solve, changing the
///        search between runs. The tabu searches are built this way.
///
/// How the runs follow one another, and what the driver may do between them, is described with
/// @c parts::Driver. How one run ended is described with @c parts::RunEnd. This class connects
/// the two: it calls the driver's hooks, notes how each run ended (@ref last_run), and offers
/// the driver the members it calls. Those members are public.
///
/// A run is one call of @ref main_loop. Unlike a phase of a search that is not driven, a run is
/// not limited by @c max_iterations. A driver that wants a budget counts its runs against it, as
/// the tabu drivers do.
///
/// @tparam Base       The search: a @c LabellingAlgorithm.
/// @tparam DriverType The driver (@c parts::Driver).
template <typename Base, typename DriverType>
class Driven : public Base {
    public:
        using resource_type = typename Base::search_type::resource_type;

        /// @brief The search, built as the base is, and its driver, built from the params.
        ///
        /// @param resource_factory The factory the search's labels are built with.
        /// @param params           The algorithm's params, which the driver reads.
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

        /// @brief Makes the runs of this solve read @p mask, in place of the arc mask the caller
        ///        set with @c set_arc_mask.
        ///
        /// Call it from the driver's @c begin. The search reads its mask through its context,
        /// which is built again at the start of each solve, so the call holds for this solve
        /// only. The driver owns @p mask, and may change it between runs.
        ///
        /// The caller's mask (@c get_arc_mask) is then not read at all. A driver that must still
        /// leave out the caller's hard arcs copies the caller's mask into its own first, as
        /// @c detail::TabuArcs::begin does.
        ///
        /// This call records no loss of proof. (The caller's mask, if it masks any arc, has
        /// already recorded one.) A driver whose mask may hide a path must call @ref lose_proof.
        void use_arc_mask(const ArcMask* mask) { this->context_.arc_mask = mask; }

        /// @brief Makes each run of this solve stop at the first path it finds.
        ///
        /// The take rule then records an end-node label as soon as the step takes it, whatever
        /// the retention, and the run ends there (@c SearchContext::should_end_run). Call it
        /// from the driver's @c begin: like the mask, the setting holds for this solve only.
        void end_runs_at_end_node() { this->context_.run_ends_at_end_node = true; }

        /// @brief Records that the solve's result is not a proof, and why (@p loss). A driver
        ///        calls it when it may have hidden the best path, for example by forbidding
        ///        arcs. The status may still be @c Complete; @c SolveResult::proven is false.
        void lose_proof(ProofLoss loss) { this->proof_.record(loss); }

        /// @brief Restarts the search from the start labels, for the next run.
        ///
        /// Every node's container and the schedule are emptied, and every label of the previous
        /// run goes back to the pool: those still waiting, and those stored aside, too. Then the
        /// start labels are created and queued again. What is not a label is kept: the
        /// incumbent, the solutions recorded, the arc mask and the completion bound.
        ///
        /// The new start labels wait until the next run takes them. So a driver that calls
        /// @c reseed and then stops leaves labels waiting, and the solve ends @c Truncated
        /// rather than @c Complete (see @c parts::Driver).
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

        /// @brief Makes one run, and notes how it ended (@ref last_run). Does nothing if the
        ///        driver's @c begin returned false.
        ///
        /// The run extends labels until one of these happens:
        /// - a step records a path that ends the run (@c end_runs_at_end_node, @c max_solutions);
        /// - a limit stops it (the time limit, an interrupt, the memory limit);
        /// - no label is waiting.
        ///
        /// In the last case, if the per-node quota or memory pressure stored labels aside, they
        /// are put back and the run goes on with them in a new phase, as a search that is not
        /// driven does. After @c max_phases phases, the labels still stored aside are put back
        /// in the waiting labels and the run stops. It then counts as
        /// @c RunEnd::ended_with_stored_aside, not @c RunEnd::exhausted.
        void main_loop() override {
            run_ = {};
            noted_in_a_step_ = false;
            if (refused_) {
                return;
            }
            in_run_ = true;
            for (size_t phase = 1;; ++phase) {
                // The run is over if a step recorded a path that ends it (end_runs_at_end_node,
                // max_solutions), or if a limit (time, interrupt, memory) stopped it with labels
                // still waiting.
                if (this->run_loop(kNoLimit) || this->number_of_waiting_labels() > 0) {
                    break;
                }
                // No label is waiting. Put back the labels stored aside, if any, for a new phase.
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
            in_run_ = false;
        }

        /// @brief True: the driver counts the runs against @c max_iterations, if it reads it.
        [[nodiscard]] bool max_iterations_counts_runs() const override { return true; }

        bool prepare_next_run(size_t number_of_runs) override {
            return !refused_ && driver_.after_run(*this, number_of_runs);
        }

        /// @brief Records @p label as the base does, and also notes its cost and path in
        ///        @ref last_run.
        ///
        /// The label is noted even when the base does not keep it as a solution: a duplicate,
        /// or a path that does not cost less than the upper bound. Its path is built only once,
        /// and used for both.
        ///
        /// Labels are recorded at two times: by a step during the run, and from the end nodes
        /// once the run is over (@c record_remaining_solutions). Once a step has noted a label,
        /// the labels recorded from the end nodes are still recorded, but not noted. So with
        /// @c end_runs_at_end_node, @ref last_run describes the path the run stopped at, even if
        /// the retention also left labels at the end nodes.
        void record_solution(const Label<resource_type>& label) override {
            const size_t before = this->solutions_.size();
            noting_ = in_run_ || !noted_in_a_step_;
            if (noting_) {
                noted_in_a_step_ = noted_in_a_step_ || in_run_;
                run_.cost = label.get_cost();
                run_.arc_ids.clear();
            }
            run_.recorded = true;
            if (label.get_cost() >= this->upper_bound_) {
                // The base drops such a label before it builds the path, which the run's end
                // still needs.
                if (noting_) {
                    run_.arc_ids = this->get_path_arc_ids(label);
                }
            } else {
                Base::record_solution(label);  // builds the path, and hands it to record_path
            }
            noting_ = false;
            run_.new_solution = run_.new_solution || this->solutions_.size() > before;
        }

        /// @brief Copies the path into @ref last_run if @ref record_solution is noting this
        ///        label, then records the path as the base does.
        void record_path(double cost, std::vector<size_t> path_arc_ids,
                         size_t end_node_id) override {
            if (noting_) {
                run_.arc_ids = path_arc_ids;
            }
            Base::record_path(cost, std::move(path_arc_ids), end_node_id);
        }

    private:
        DriverType driver_;
        parts::RunEnd run_;
        bool refused_ = false;          ///< the driver's @c begin returned false: no run is made
        bool in_run_ = false;           ///< true during @ref main_loop: a record comes from a step
        bool noted_in_a_step_ = false;  ///< a step of the last run noted a label in @ref run_
        bool noting_ = false;           ///< the label being recorded is noted in @ref run_
};

}  // namespace rcspp::detail
