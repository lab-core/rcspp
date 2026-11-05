// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/arc_mask.hpp"
#include "rcspp/algorithm/proof_record.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp::parts {

/// @brief A driver: runs one search several times in one solve, changing it between runs.
///
/// It is built from the algorithm's params. @c begin is called at the start of each solve, once
/// the search is ready: it may mask arcs (@c use_arc_mask), end each run at its first path
/// (@c end_runs_at_end_node), and returns whether to run at all. @c after_run is called after
/// each run (@p runs_done counts them, from 1): it reads how the run ended (@c last_run), may
/// change the search, starts the next run with @c reseed, and returns whether to run again. A
/// driver that is not exact says so with @c lose_proof.
///
/// @tparam DriverType The driver.
/// @tparam SearchType The driven search (@c detail::Driven).
template <typename DriverType, typename SearchType>
concept Driver = std::constructible_from<DriverType, const AlgorithmBaseParams&> &&
                 requires(DriverType driver, SearchType& search, size_t runs_done) {
                     { driver.begin(search) } -> std::same_as<bool>;
                     { driver.after_run(search, runs_done) } -> std::same_as<bool>;
                 };

/// @brief How one run of a driven search ended.
struct RunEnd {
        /// @brief Whether the run recorded an end-node label: with @c end_runs_at_end_node, the
        ///        label it ended at.
        bool recorded = false;
        /// @brief Whether the run ended with no label waiting: not stopped by a budget, an
        ///        interrupt or the memory limit.
        bool exhausted = false;
        /// @brief Whether the run added a solution the solve did not have.
        bool new_solution = false;
        /// @brief The cost of the end-node label the run recorded last; infinite when none.
        double cost = std::numeric_limits<double>::infinity();
        /// @brief Its path's arc ids, in path order: recorded as a solution or not (a duplicate,
        ///        or not below the upper bound).
        std::vector<size_t> arc_ids;
};

}  // namespace rcspp::parts

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
                          "runs_done)");
        }

        /// @brief The graph being solved.
        [[nodiscard]] const Graph<resource_type>& graph() const { return *this->graph_; }

        /// @brief The algorithm's params.
        [[nodiscard]] const AlgorithmBaseParams& params() const { return this->params_; }

        /// @brief The solve's cost upper bound.
        [[nodiscard]] double upper_bound() const { return this->upper_bound_; }

        /// @brief How many solutions the solve has recorded.
        [[nodiscard]] size_t solution_count() const { return this->solutions_.size(); }

        /// @brief How the last run ended.
        [[nodiscard]] const parts::RunEnd& last_run() const { return run_; }

        /// @brief Has the runs of this solve read @p mask instead of the one @c set_arc_mask set;
        ///        the driver owns it, and may change it between runs.
        void use_arc_mask(const ArcMask* mask) { this->ctx_.arc_mask = mask; }

        /// @brief Ends each run of this solve once it records an end-node label.
        void end_runs_at_end_node() { this->ctx_.run_ends_at_end_node = true; }

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
            run_.exhausted = this->waiting_labels() == 0;
        }

        bool next_run(size_t runs_done) override {
            return !refused_ && driver_.after_run(*this, runs_done);
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
