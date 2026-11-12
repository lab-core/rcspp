// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <list>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm_params.hpp"
#include "rcspp/algorithm/limits.hpp"
#include "rcspp/algorithm/search_direction.hpp"
#include "rcspp/algorithm/solution.hpp"
#include "rcspp/algorithm/solve_result.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label_list.hpp"
#include "rcspp/label/label_pool.hpp"
#include "rcspp/resource/concrete/numerical_resource.hpp"
#include "rcspp/utils/memory.hpp"
#include "rcspp/utils/timer.hpp"
#include "rcspp/utils/utils.hpp"
#include "rcspp/validation/pre_solve_plan.hpp"

namespace rcspp {

template <typename ResourceType>
using LabelIterator = std::list<Label<ResourceType>*>::iterator;

template <typename ResourceType>
using LabelIteratorPair =
    std::pair<Label<ResourceType>*, typename std::list<Label<ResourceType>*>::iterator>;

namespace detail {

/// @brief Cuts a queue of labels waiting to be extended down to its @p keep best entries, best
///        meaning non-dominated first, then cheaper first.
///
/// What happens to the entries cut is the caller's: a dominated one is usually released, a
/// non-dominated one kept aside for a later phase or dropped.
///
/// @param queue The queue; sorted, best first, when anything is cut (and @p sort is set).
/// @param keep  How many entries stay.
/// @param sort  Whether to sort first; without it the queue's own order decides what stays.
/// @return The entries cut, best first; empty when @p queue held at most @p keep.
template <typename ResourceType>
std::list<LabelIteratorPair<ResourceType>> split_off_excess(
    std::list<LabelIteratorPair<ResourceType>>* queue, size_t keep, bool sort = true) {
    std::list<LabelIteratorPair<ResourceType>> excess;
    if (queue->size() <= keep) {
        return excess;
    }
    if (sort) {
        queue->sort([](const LabelIteratorPair<ResourceType>& lhs,
                       const LabelIteratorPair<ResourceType>& rhs) {
            if (lhs.first->dominated != rhs.first->dominated) {
                return !lhs.first->dominated;  // non-dominated first
            }
            return lhs.first->get_cost() < rhs.first->get_cost();  // then cheaper first
        });
    }
    excess.splice(excess.end(),
                  *queue,
                  std::next(queue->begin(), static_cast<std::ptrdiff_t>(keep)),
                  queue->end());
    return excess;
}

}  // namespace detail

template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>>
    requires ResourceTypeConcept<ResourceType>
class Algorithm {
    public:
        Algorithm(ResourceFactory<ResourceType>* resource_factory,
                  AlgorithmParams<LabelContainerType> params)
            : label_pool_(std::make_unique<LabelFactory<ResourceType>>(resource_factory)),
              graph_(nullptr),
              params_(std::move(params)) {
            params_.check();
        }

        virtual ~Algorithm() = default;

        /**
         * @brief Whether the last @ref solve processed every label, so that its result is optimal.
         *
         * The default implementation answers whether the last solve ended with status
         * @c AlgorithmStatus::Complete. It is recorded when the solve ends, before
         * @c release_after_solve (the default) empties the label queues: reading
         * @ref number_of_waiting_labels afterwards would answer 0 after a timeout or any other
         * early stop.
         * @c false before the first solve. A heuristic ends @c Complete only when it ran out of
         * paths to try: @c GreedyAlgorithm then enumerated every path, and
         * @c TabuSearchAlgorithm found none with no arc tabu. @c ImprovingTabuSearch and
         * @c DiversificationSearch override it to answer @c false.
         *
         * @return true if the last solve processed every label, false otherwise.
         */
        [[nodiscard]] virtual bool is_optimal() const { return last_solve_complete_; }

        virtual SolveResult solve(const Graph<ResourceType>* graph, double cost_upper_bound) {
            require_supported_direction();
            // The checks this direction needs, whoever calls: ResourceGraph::solve ran them
            // already, before preprocessing, but a direct call or an algorithm running another
            // inside it did not.
            SolveContext context{.upper_bound = cost_upper_bound};
            PreSolvePlan<ResourceType>(direction()).enforce(*graph, context);

            // initialization
            Timer timer(true);
            timed_out_ = false;
            last_solve_complete_ = false;
            solve_timer_ = &timer;
            memory_limit_.resolve(params_.max_memory_gb,
                                  params_.limit_to_available_ram,
                                  params_.limit_to_total_ram,
                                  params_.memory_limit_fraction,
                                  params_.memory_pressure_fraction,
                                  params_.memory_check_interval);
            initialize(graph, cost_upper_bound);

            // initialize labels
            this->initialize_labels();

            size_t num_phases = 0;
            while (!should_end_loop() && number_of_waiting_labels() > 0) {
                // main labeling loop
                main_loop();

                // extract solutions any remaining solutions
                record_remaining_solutions();

                // prepare next phase (if any)
                if (++num_phases < params_.max_phases) {
                    prepare_next_phase();
                } else {
                    break;
                }
            }

            solve_timer_ = nullptr;

            if (LOG_DEBUG_ACTIVE()) {
                LOG_DEBUG("Total number of extended labels: ", number_of_extended_labels_, "\n");
                print_labels();
            }

            // recover solutions
            std::vector<Solution> solutions;
            solutions.reserve(solutions_.size());
            for (auto&& solution : solutions_) {
                solutions.push_back(std::move(solution));
            }

            // Put back the labels a phase held back (truncated labeling, memory pressure) before
            // the status is read: they were never extended, so a run that used up its phases with
            // labels held back is Truncated, not Complete, whether or not the labels are released
            // below. When they are kept, this also readies the next solve().
            prepare_next_phase();

            // sort solutions
            std::ranges::sort(solutions,
                              [](const Solution& a, const Solution& b) { return a.cost < b.cost; });

            LOG_DEBUG("Number of solutions before resize: ", solutions.size(), '\n');
            LOG_DEBUG("Min cost=",
                      solutions.empty() ? cost_upper_bound : solutions.front().cost,
                      "\n");
            LOG_DEBUG("Total time=", timer.elapsed_seconds(), " sec.\n");

            // resize solutions if needed
            if (solutions.size() > params_.max_solutions) {
                solutions.resize(params_.max_solutions);
            }

            // Determine the exit status. "All labels processed" (number_of_waiting_labels() == 0)
            // means the search finished and the result is optimal, so it takes precedence over the
            // early-stop reasons below: a timeout / interrupt / memory flag must NOT downgrade a
            // finished, optimal run. This matters because is_interrupted() and
            // memory_limit_.is_exceeded() are re-evaluated here — the external stop callback may
            // have flipped, or RSS may exceed the limit because of the retained solutions — even
            // though the loop actually exited by exhausting all labels. The remaining branches
            // describe why the search stopped *early* (labels still remain). A search that
            // abandoned labels it will never extend (has_abandoned_labels()) did not process every
            // label, even with an empty queue.
            AlgorithmStatus status;
            if (number_of_waiting_labels() == 0 && !has_abandoned_labels()) {
                status = AlgorithmStatus::Complete;
            } else if (timed_out_) {
                status = AlgorithmStatus::Timeout;
            } else if (is_interrupted()) {
                status = AlgorithmStatus::Interrupted;
            } else if (memory_limit_.is_exceeded()) {
                status = AlgorithmStatus::MemoryLimit;
            } else if (solutions.size() >= params_.max_solutions) {
                status = AlgorithmStatus::MaxSolutions;
            } else {
                status = AlgorithmStatus::Truncated;
            }
            // Recorded now: releasing the label memory below empties the queues, after which
            // number_of_waiting_labels() would read 0 whatever the run did.
            last_solve_complete_ = status == AlgorithmStatus::Complete;

            // Optionally release label memory so RAM is reclaimed when the caller returns.
            if (params_.release_after_solve) {
                release_label_memory();
            }

            SolveResult result{.solutions = std::move(solutions), .status = status};
            annotate(&result);
            return result;
        }

        /// @brief The direction this algorithm searches in, as its params set it.
        [[nodiscard]] SearchDirection direction() const { return params_.direction; }

        /// @brief The directions this algorithm can search in. Forward only by default.
        [[nodiscard]] virtual std::vector<SearchDirection> supported_directions() const {
            return {SearchDirection::Forward};
        }

        [[nodiscard]] bool is_interrupted() const {
            return params_.should_stop && params_.should_stop();
        }

        /// @brief Read-only access to the label pool, for diagnostics and tests.
        [[nodiscard]] const LabelPool<ResourceType>& get_label_pool() const { return label_pool_; }

        /// @brief How many labels the last solve extended; a machine-independent work measure.
        ///
        /// @return The number of label extensions performed.
        [[nodiscard]] size_t get_number_of_extended_labels() const {
            return number_of_extended_labels_;
        }

        /// @brief Whether memory pressure fired at least once during the last solve.
        ///
        /// If so, the result may not be optimal; `could_be_non_optimal()` reads params only and
        /// does not reflect this.
        ///
        /// @return `true` when `on_memory_pressure()` was called during the last solve.
        [[nodiscard]] bool memory_pressure_was_triggered() const {
            return memory_pressure_triggered_;
        }

    protected:
        virtual void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) {
            if (!graph->get_sorted_nodes().empty() && !graph->are_nodes_sorted()) {
                LOG_FATAL(
                    "Graph has a sorted nodes structure that is not correctly sorted. Do not "
                    "manipulate the pos index of the nodes.\n");
                throw std::runtime_error(
                    "Graph has a sorted nodes structure that is not correctly sorted. Do not "
                    "manipulate the pos index of the nodes.");
            }

            graph_ = graph;
            upper_bound_ = cost_upper_bound;
            incumbent_ = cost_upper_bound;
            label_pool_.clear();
            solutions_.clear();
            number_of_extended_labels_ = 0;
            quota_ = params_.max_extended_labels_per_node;
            memory_pressure_triggered_ = false;
        }

        /// @brief Throws unless this algorithm supports the direction its params ask for.
        ///
        /// @throws std::invalid_argument when @ref direction() is not one of
        ///         @ref supported_directions().
        void require_supported_direction() const {
            const auto supported = supported_directions();
            if (std::ranges::find(supported, direction()) != supported.end()) {
                return;
            }
            std::string list;
            for (const auto supported_direction : supported) {
                if (!list.empty()) {
                    list += ", ";
                }
                list += to_string(supported_direction);
            }
            throw std::invalid_argument(template_name(*this) + " does not support the " +
                                        std::string(to_string(direction())) +
                                        " direction (it supports: " + list + ")");
        }

        /// @brief Adds diagnostics to the result, just before it is returned.
        ///
        /// The base reports memory pressure; overrides must call `Base::annotate(result)`.
        ///
        /// @param result The result about to be returned; never null.
        virtual void annotate(SolveResult* result) const {
            result->memory_pressure_triggered = memory_pressure_triggered_;
        }

        /// @brief Hook called when @ref memory_limit_.is_under_pressure() becomes true.
        ///
        /// The default implementation does nothing.  Subclasses that own
        /// unprocessed label queues (e.g. PushingDominanceAlgorithm) override
        /// this to trim those queues, slowing further RSS growth before the
        /// hard limit is hit.
        virtual void on_memory_pressure() {}

        /// @brief A main loop's periodic memory check: logs, trims the queues under pressure
        ///        (@ref on_memory_pressure), and says whether the loop must stop.
        ///
        /// The memory is read every @c memory_check_interval iterations only, never at 0.
        ///
        /// @param iteration The main loop's iteration count.
        /// @return @c true when the memory limit is exceeded.
        bool check_memory(size_t iteration) {
            switch (memory_limit_.check(iteration)) {
                case MemoryState::Exceeded:
                    LOG_WARN("Memory limit (",
                             memory_limit_.effective_limit / kMB,
                             " MB) exceeded (current: ",
                             MemoryInfo::process_bytes() / kMB,
                             " MB). Stopping early.\n");
                    return true;
                case MemoryState::UnderPressure:
                    LOG_INFO("Memory pressure: ",
                             MemoryInfo::process_bytes() / kMB,
                             " MB / ",
                             memory_limit_.effective_limit / kMB,
                             " MB. Trimming label queues.\n");
                    on_memory_pressure();
                    return false;
                case MemoryState::Ok:
                    return false;
            }
            return false;
        }

        /// @brief Release all label memory held by the pool and label containers.
        ///
        /// Called at the end of solve() so that RSS is reclaimed as soon as the
        /// caller returns.  The default implementation frees the pool (including
        /// shrink_to_fit).  Subclasses override this to also clear their own
        /// label pointer containers (non-dominated sets, unprocessed queues).
        virtual void release_label_memory() { label_pool_.release(); }

        // ── Core virtuals ───────────────────────────────────────────────────

        virtual void initialize_labels() = 0;

        [[nodiscard]] virtual size_t number_of_waiting_labels() const = 0;

        /// @brief Whether the last solve abandoned labels it will never extend.
        ///
        /// Such labels are not in the queues, so @ref number_of_waiting_labels does not count them,
        /// and an empty queue does not mean that every label was processed. A search that stores
        /// the labels a quota or memory pressure cuts (for a later phase) needs nothing here:
        /// @ref prepare_next_phase puts them back before the status is read. The default answers
        /// @c false.
        ///
        /// @return @c true when a label was cut and left unextended.
        [[nodiscard]] virtual bool has_abandoned_labels() const { return false; }

        virtual void prepare_next_phase() {}

        virtual void main_loop() = 0;

        /// @brief Records the solutions still sitting at the end nodes when the loop ends.
        ///
        /// Virtual so a bidirectional search can add backward and joined paths.
        virtual void record_remaining_solutions() {
            auto at_end_nodes = this->labels_at_end_nodes();
            for (const auto* label : at_end_nodes) {
                this->record_solution(*label);
            }
        }

        [[nodiscard]] virtual std::list<Label<ResourceType>*> labels_at_end_nodes() const = 0;

        virtual void print_labels() const {}

        virtual std::vector<size_t> get_path_arc_ids(const Label<ResourceType>& label) = 0;

        virtual void record_solution(const Label<ResourceType>& end_label) {
            if (end_label.get_cost() >= upper_bound_) {
                return;
            }

            record_path(end_label.get_cost(),
                        this->get_path_arc_ids(end_label),
                        end_label.get_end_node()->id);
        }

        /// @brief Build and record a Solution from an explicit path.
        ///
        /// Used when no single label holds the path (e.g. joined pairs). Paths costing at least
        /// `upper_bound_` are dropped here, for every caller.
        ///
        /// @param cost         Total cost of the path.
        /// @param path_arc_ids Arc ids of the path, in traversal order.
        /// @param end_node_id  Id of the node the path ends at.
        virtual void record_path(double cost, std::vector<size_t> path_arc_ids,
                                 size_t end_node_id) {
            if (cost >= upper_bound_) {
                return;
            }
            if (path_arc_ids.empty()) {
                return;
            }

            // Its hash reads the arc ids only, so a duplicate is caught before the column is built.
            auto sol = Solution(cost, {}, std::move(path_arc_ids));
            if (solutions_.contains(sol)) {
                return;
            }

            // Build column: sum original arc costs and aggregate constraint coefficients
            Column column;
            std::unordered_map<size_t, long double> row_map;
            std::vector<size_t> path_node_ids;
            path_node_ids.reserve(sol.path_arc_ids.size() + 1);
            for (size_t arc_id : sol.path_arc_ids) {
                const auto* arc = this->graph_->get_arc(arc_id);
                path_node_ids.push_back(arc->origin->id);
                column.cost += arc->cost;
                for (const auto& row : arc->rows) {
                    row_map[row.index] += row.coefficient;
                }
            }
            path_node_ids.push_back(end_node_id);
            column.rows.reserve(row_map.size());
            for (auto& [idx, coef] : row_map) {
                column.rows.push_back({idx, coef});
            }
            std::sort(column.rows.begin(), column.rows.end(), [](const Row& a, const Row& b) {
                return a.index < b.index;
            });

            sol.path_node_ids = std::move(path_node_ids);
            sol.column = std::move(column);
            solutions_.insert(std::move(sol));
        }

        /// @brief Returns true when the wall-clock timeout has been exceeded.
        ///
        /// Sets timed_out_ on the first call that exceeds the limit so subsequent
        /// calls are O(1) (no timer read).  Safe to call only during solve().
        bool is_time_out() {
            if (!timed_out_ && solve_timer_ != nullptr &&
                solve_timer_->elapsed_seconds() >= params_.timeout_s) {
                timed_out_ = true;
            }
            return timed_out_;
        }

        /// @brief Whether a loop must stop: @p iteration has reached @c max_iterations (0, the
        ///        default, skips this check), @c max_solutions solutions are held, the time limit
        ///        has passed, or the caller's @c should_stop callback asks to stop
        ///        (@ref is_interrupted).
        ///
        /// Every loop asks it: @ref solve's loop over phases, and each algorithm's own loop.
        ///
        /// @param iteration The loop's iteration count (0 skips the iteration check).
        bool should_end_loop(size_t iteration = 0) {
            return iteration >= params_.max_iterations ||
                   solutions_.size() >= params_.max_solutions || is_time_out() || is_interrupted();
        }

        LabelPool<ResourceType> label_pool_;
        const Graph<ResourceType>* graph_;
        const AlgorithmParams<LabelContainerType> params_;
        MemoryLimitHelper memory_limit_;  ///< Resolved at the start of each solve().

        /// @brief Effective per-node extension cap.
        ///
        /// Starts at @ref AlgorithmBaseParams::max_extended_labels_per_node at the
        /// beginning of each solve and is tightened to
        /// @ref AlgorithmBaseParams::memory_pressure_max_labels_per_node when
        /// @ref on_memory_pressure() fires.  Subclasses use this instead of
        /// @c params_.max_extended_labels_per_node wherever the per-node limit is enforced.
        size_t quota_ = kNoLimit;

        /// @brief True after on_memory_pressure() has been called at least once this solve.
        ///
        /// Used by subclasses to distinguish a first pressure event (prune & store aside)
        /// from subsequent ones (prune & discard stored labels to free more memory).
        bool memory_pressure_triggered_ = false;

        /// @brief The cost upper bound the caller gave this solve: a path is recorded only when
        ///        it costs less. Fixed for the solve.
        double upper_bound_ = std::numeric_limits<double>::infinity();
        /// @brief The incumbent: the cost of the cheapest path to an end node found so far in
        ///        this solve, or @ref upper_bound_ before any. It only falls; the incumbent prune
        ///        (@c prune_by_incumbent) and the join compare with it.
        double incumbent_ = std::numeric_limits<double>::infinity();
        std::unordered_set<Solution> solutions_;

        size_t number_of_extended_labels_ = 0;

        bool timed_out_ = false;
        bool last_solve_complete_ = false;    ///< Whether the last solve() ended Complete.
        const Timer* solve_timer_ = nullptr;  ///< Points to solve()'s timer; null outside solve().
};

}  // namespace rcspp
