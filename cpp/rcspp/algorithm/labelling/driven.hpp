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

/// @brief A labelling search run several times by @p DriverType (@c parts::Driver), which
///        changes it between runs.
///
/// Each run takes labels until none waits, a stop applies, or (with @c end_runs_at_end_node) it
/// records an end-node label; the solve's loop then asks the driver whether to run again, in
/// place of the phases. @c max_iterations does not count a run's labels: a driver counts its
/// runs against it, if it reads it. The members the driver calls are public.
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

        /// @brief Has the runs of this solve read @p mask instead of the one @c set_arc_mask set;
        ///        the driver owns it, and may change it between runs.
        void use_arc_mask(const ArcMask* mask) { this->context_.arc_mask = mask; }

        /// @brief Ends each run of this solve once it records an end-node label.
        void end_runs_at_end_node() { this->context_.run_ends_at_end_node = true; }

        /// @brief Records that the solve's result is not a proof, for @p loss.
        void lose_proof(ProofLoss loss) { this->proof_.record(loss); }

        /// @brief Starts a new run from the start labels: the schedule is emptied, and the pool
        ///        takes back every label of the previous run.
        void reseed() {
            this->search_.clear();
            this->initialize_labels();
        }

    protected:
        void initialize(const Graph<resource_type>* graph, double cost_upper_bound) override {
            Base::initialize(graph, cost_upper_bound);
            run_ = {};
            refused_ = !driver_.begin(*this);
        }

        /// @brief One run, unless the driver refused to run.
        void main_loop() override {
            run_ = {};
            if (refused_) {
                return;
            }
            this->run_loop(kNoLimit);
            run_.exhausted = this->number_of_waiting_labels() == 0;
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
