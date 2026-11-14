// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
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
#include "rcspp/algorithm/arc_mask.hpp"
#include "rcspp/algorithm/dominance_mask.hpp"
#include "rcspp/algorithm/labelling/parts/admission/admission.hpp"
#include "rcspp/algorithm/labelling/parts/extension_outcome.hpp"
#include "rcspp/algorithm/limits.hpp"
#include "rcspp/algorithm/proof_record.hpp"
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

namespace detail {

template <typename ResourceType>
struct SearchContext;
template <typename ResourceType>
class HalfWay;
enum class Role : std::uint8_t;

/// @brief The label an entry of a queue holds: a @c parts::StoredLabel's @c label, or a pair's
///        first member.
template <typename EntryType>
[[nodiscard]] auto* label_of(const EntryType& entry) {
    if constexpr (requires { entry.label; }) {
        return entry.label;
    } else {
        return entry.first;
    }
}

/// @brief Cuts a queue of labels waiting to be extended down to its @p keep best entries, best
///        meaning non-dominated first, then cheaper first.
///
/// What happens to the entries cut is the caller's: a dominated one is usually released, a
/// non-dominated one kept aside for a later phase or dropped.
///
/// @tparam EntryType A queue entry: a label and its position in its node's container.
/// @param queue The queue; sorted, best first, when anything is cut (and @p sort is set).
/// @param keep  How many entries stay.
/// @param sort  Whether to sort first; without it the queue's own order decides what stays.
/// @return The entries cut, best first; empty when @p queue held at most @p keep.
template <typename EntryType>
std::list<EntryType> split_off_excess(std::list<EntryType>* queue, size_t keep, bool sort = true) {
    std::list<EntryType> excess;
    if (queue->size() <= keep) {
        return excess;
    }
    if (sort) {
        queue->sort([](const EntryType& lhs, const EntryType& rhs) {
            const auto* left = label_of(lhs);
            const auto* right = label_of(rhs);
            if (left->dominated != right->dominated) {
                return !left->dominated;  // non-dominated first
            }
            return left->get_cost() < right->get_cost();  // then cheaper first
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

        /// @brief Whether the last @ref solve's result is a proof of optimality
        ///        (@c SolveResult::proven).
        ///
        /// The default implementation answers whether the last solve ended with status
        /// @c AlgorithmStatus::Complete, with nothing lost that the status cannot see: no arc
        /// masked, an exact admission rule, no memory pressure (@c ProofLoss). It is recorded when
        /// the solve ends, before @c release_after_solve (the default) empties the label queues:
        /// reading @ref number_of_waiting_labels afterwards would answer 0 after a timeout or any
        /// other early stop. @c false before the first solve. A heuristic ends @c Complete only
        /// when it ran out of paths to try: @c strategy::Greedy then enumerated every path, and the
        /// tabu searches found none before any arc was tabu (a tabu arc loses the proof:
        /// @c ProofLoss::HeuristicDriver). @c DiversificationSearch overrides it to answer
        /// @c false.
        ///
        /// @return true if the last solve's result is a proof, false otherwise.
        [[nodiscard]] virtual bool is_optimal() const { return last_solve_proven_; }

        /// @brief Solves @p graph: returns the paths found that cost less than
        ///        @p cost_upper_bound, cheapest first, and why the search ended.
        ///
        /// A solve checks the direction and the model, resets this algorithm's state
        /// (@ref initialize) and creates the start labels (@ref initialize_labels). Then it
        /// repeats runs until no label waits or a stop applies (@ref should_end_loop): each run
        /// extends labels (@ref main_loop), records the paths at the end nodes
        /// (@ref record_remaining_solutions), and asks @ref prepare_next_run whether another run
        /// follows (the next phase, or a driver's next run). It then puts back the labels still
        /// stored aside (@ref prepare_next_phase), sets the status (@ref AlgorithmStatus) and
        /// whether the result is a proof (@c SolveResult::proven), and releases the label memory
        /// unless @c release_after_solve is false. Each solve starts afresh: nothing of one
        /// solve's labels is reused by the next.
        ///
        /// @param graph            The graph; it must not change during the solve.
        /// @param cost_upper_bound Only paths cheaper than this are returned; +infinity for no
        ///                         bound.
        /// @return The paths, each once, at most @c max_solutions, cheapest first; and the
        ///         status.
        /// @throws std::invalid_argument when the algorithm does not support the direction.
        /// @throws ModelRefused when the model checks of a backward or bidirectional search
        ///         fail.
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
            last_solve_proven_ = false;
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

            size_t runs = 0;
            while (!should_end_loop() && number_of_waiting_labels() > 0) {
                // main labeling loop
                main_loop();

                // record the paths the run left at the end nodes
                record_remaining_solutions();

                // the next phase, or a driver's next run (if any)
                if (!prepare_next_run(++runs)) {
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
            // below.
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
            // means the search finished, so it takes precedence over the
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
            if (memory_pressure_triggered_) {
                proof_.record(ProofLoss::MemoryPressure);
            }
            const bool complete = status == AlgorithmStatus::Complete;
            last_solve_proven_ = complete && proof_.is_intact();

            // Optionally release label memory so RAM is reclaimed when the caller returns.
            if (params_.release_after_solve) {
                release_label_memory();
            }

            SolveResult result{
                .solutions = std::move(solutions),
                .status = status,
                .proven = last_solve_proven_,
                .proof_lost_by = complete ? to_string(proof_.first_loss()) : to_string(status)};
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

        /// @brief Sets the arc mask later solves read: they leave out the arcs @p mask makes hard
        ///        (a hard arc), as if they were not in the graph, and a child filter may avoid
        ///        those it makes soft (a soft arc); @c nullptr (the default) leaves every arc in.
        ///
        /// A labelling search never extends a label along a hard arc. The graph is not edited:
        /// arc ids, and the bounds computed on the whole graph, are unchanged, so the paths
        /// returned name the graph's own arcs. The mask is read at the start of each solve, and
        /// not owned: it must outlive the solves that read it. A soft arc changes nothing for a
        /// search whose schedule does not read it (every exact search). A mask that is not all
        /// open (any hard or soft arc) costs the solve its proof (@c ProofLoss::ArcsMasked), a
        /// soft-only one included.
        ///
        /// @param mask The mask, sized for the graph's arc ids (@c Graph::get_arc_id_bound).
        void set_arc_mask(const ArcMask* mask) { arc_mask_ = mask; }

        /// @brief The mask later solves read; @c nullptr when none was set.
        [[nodiscard]] const ArcMask* get_arc_mask() const { return arc_mask_; }

        /// @brief Read-only access to the label pool, for diagnostics and tests.
        [[nodiscard]] const LabelPool<ResourceType>& get_label_pool() const { return label_pool_; }

        /// @brief How many extensions the last solve made that drew a label from the pool: every
        ///        extension of a label along an arc but those masked or unreachable (the
        ///        infeasible and the rejected ones count). A machine-independent work measure.
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
        template <typename, typename, typename>
        friend class DiversificationSearch;

        /// @brief Promises that the graph, costs included, has not changed since the last solve,
        ///        so that the completion bound computed then can be kept
        ///        (@c LabellingAlgorithm::refresh_completion_bound), until this is set back to @c
        ///        false.
        ///
        /// Protected: only @ref DiversificationSearch makes this promise, between its inner
        /// solves. A wrong promise leaves a stale bound, so wrong pruning and ordering. The arc
        /// mask is not part of the promise: the bound does not read it.
        ///
        /// @param unchanged Whether the graph stays unchanged.
        void set_graph_unchanged(bool unchanged) { graph_unchanged_ = unchanged; }

        /// @brief Whether the graph is promised unchanged since the last solve
        ///        (@ref set_graph_unchanged).
        [[nodiscard]] bool is_graph_unchanged() const { return graph_unchanged_; }

        /// @brief Starts a solve: stores the graph and the upper bound, sets the incumbent to
        ///        the upper bound, clears the pool and the solutions, resets the counters, the
        ///        quota, the pressure flag and the proof record, records the proof losses known
        ///        at the start (an arc mask, a prune on a cost that is not additive), and builds
        ///        the relaxed rule's mask (@ref admission_mask).
        ///
        /// An override calls this first, then sets up its own search(es).
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
            proof_.reset();
            if (arc_mask_ != nullptr && !arc_mask_->empty()) {
                proof_.record(ProofLoss::ArcsMasked);
            }
            if (params_.prune_by_incumbent && !is_cost_additive(*graph)) {
                proof_.record(ProofLoss::IncumbentPrune);
            }
            dominance_mask_ = admission_mask(*graph);
        }

        /// @brief The mask the relaxed rule compares over, sized for @p graph's resource; empty
        ///        under any other rule. The k-best rule's k is checked too.
        ///
        /// @throws std::invalid_argument when `dominance_ignored_components` names a component
        ///         the model does not have, or when `KBest` would keep no label
        ///         (`max_admitted_labels_per_node` 0).
        [[nodiscard]] DominanceMask admission_mask(const Graph<ResourceType>& graph) const {
            if (params_.admission_rule == parts::AdmissionRule::KBest &&
                params_.max_admitted_labels_per_node == 0) {
                throw std::invalid_argument(
                    "max_admitted_labels_per_node is 0, so the KBest rule would keep no label; "
                    "give it at least 1");
            }
            const auto& ignored = params_.dominance_ignored_components;
            if (params_.admission_rule != parts::AdmissionRule::Relaxed || ignored.empty()) {
                return {};
            }
            const size_t count = number_of_components(graph);
            if (*ignored.rbegin() >= count) {
                throw std::invalid_argument(
                    "dominance_ignored_components names component " +
                    std::to_string(*ignored.rbegin()) + ", but the model has " +
                    std::to_string(count) +
                    " component(s), numbered from 0 in the order of the ResourceGraph's type "
                    "parameters, then registration order within a type");
            }
            DominanceMask mask(count);
            for (const size_t component : ignored) {
                mask.ignore(component);
            }
            return mask;
        }

        /// @brief How many components a resource of @p graph has, read off any node's resource.
        ///
        /// @return The count: 1 for a resource that is not a composition, 0 when no node carries
        ///         a resource.
        [[nodiscard]] static size_t number_of_components(const Graph<ResourceType>& graph) {
            for (const size_t node_id : graph.get_node_ids()) {
                const auto* node = graph.get_node(node_id);
                if (node == nullptr || node->resource == nullptr) {
                    continue;
                }
                if constexpr (requires { node->resource->get_components(); }) {
                    size_t count = 0;
                    node->resource->for_each_component(
                        [&count](const auto& /*component*/) { ++count; });
                    return count;
                } else {
                    return 1;
                }
            }
            return 0;
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

        /// @brief Called by @ref check_memory each time a periodic memory check finds the
        ///        pressure threshold reached: possibly several times in one solve.
        ///
        /// The default does nothing. The labelling algorithms override it to call
        /// @ref relieve_memory_pressure on each of their searches.
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

        /// @brief Counts one extension of a label along an arc
        ///        (@ref get_number_of_extended_labels), and logs the progress every
        ///        @ref kProgressReportInterval extensions.
        ///
        /// Every extension that drew a label counts towards @ref get_number_of_extended_labels:
        /// all but the masked and the unreachable ones.
        ///
        /// @param outcome How the extension ended.
        void count_extension(parts::ExtensionOutcome outcome) {
            if (outcome == parts::ExtensionOutcome::Masked ||
                outcome == parts::ExtensionOutcome::Unreachable) {
                return;
            }
            if (++number_of_extended_labels_ % kProgressReportInterval == 0) {
                print_labels();
                LOG_DEBUG("Made ", number_of_extended_labels_, " extensions so far...\n");
            }
        }

        /// @brief Records the path a backward label at a source holds.
        ///
        /// A backward label is extended from a sink toward the sources: its path runs from the
        /// label's own node (here a source) to the sink its search started from, which is the
        /// destination of the path's last arc. A path costing at least the upper bound is dropped
        /// (@ref record_path).
        ///
        /// @param label A backward label at a source.
        void record_backward_solution(const Label<ResourceType>& label) {
            std::vector<size_t> arc_ids;
            append_suffix_arc_ids(label, &arc_ids);
            if (arc_ids.empty()) {
                return;
            }
            const size_t end_node_id = graph_->get_arc(arc_ids.back())->destination->id;
            record_path(label.get_cost(), std::move(arc_ids), end_node_id);
        }

        /// @brief Builds the context one search of this solve reads (@c detail::SearchContext).
        ///
        /// Every pointer in it points to the graph being solved, into this algorithm (its pool,
        /// params, incumbent, quota and solutions), into @p search (its completion bound), or to
        /// the arc mask set with @ref set_arc_mask: the context is valid while this algorithm, the
        /// search and the mask live. It also says whether the
        /// incumbent prune adds the completion bound (@c prune_adds_completion_bound: the prune is
        /// on and the cost is additive, @ref is_cost_additive).
        ///
        /// @param search    The search (a @c detail::Search).
        /// @param role      Whether it runs alone or as a side of a bidirectional search.
        /// @param number_of_abandoned_labels For a bidirectional side only (null otherwise): the
        /// counter of the
        ///                  labels its cuts abandon.
        /// @param half_way  For a bidirectional side only (null otherwise): the half-way point
        ///                  past which the side does not extend a label.
        template <typename SearchType>
        detail::SearchContext<ResourceType> make_context(
            // NOLINTNEXTLINE(readability-non-const-parameter): stored in the context
            SearchType& search, detail::Role role, size_t* number_of_abandoned_labels = nullptr,
            const detail::HalfWay<ResourceType>* half_way = nullptr) {
            return {
                .graph = graph_,
                .pool = &label_pool_,
                .params = &params_,
                .upper_bound = upper_bound_,
                .incumbent = &incumbent_,
                .quota = &quota_,
                .solutions = &solutions_,
                .admission = {.rule = params_.admission_rule,
                              .dominance_mask = &dominance_mask_,
                              .max_labels_per_node = params_.max_admitted_labels_per_node},
                .completion_bound = &search.completion_bound,
                .arc_mask = arc_mask_,
                .role = role,
                .prune_adds_completion_bound =
                    params_.prune_by_incumbent && is_cost_additive(*graph_),
                .number_of_abandoned_labels = number_of_abandoned_labels,
                .half_way = half_way,
            };
        }

        /// @brief Whether the cost of @p graph's model is additive (@c Resource::is_cost_additive,
        ///        read off any node's resource): the completion bound is then a valid lower bound
        ///        on the cost still to pay, which the incumbent prune can add.
        static bool is_cost_additive(const Graph<ResourceType>& graph) {
            for (const size_t node_id : graph.get_node_ids()) {
                const auto* node = graph.get_node(node_id);
                if (node != nullptr && node->resource != nullptr) {
                    return node->resource->is_cost_additive();
                }
            }
            return true;
        }

        /// @brief Relieves memory pressure on one search.
        ///
        /// In order: lowers the per-node quota to @c memory_pressure_max_labels_per_node (never
        /// raising it); on every call but the first of the solve, releases every label the
        /// search has stored aside in this phase, whether the quota or an earlier trim stored
        /// it (those labels are lost); sets the pressure flag; and has the schedule trim its
        /// waiting labels. What the trim takes off is stored aside, or abandoned and counted on
        /// a bidirectional side (which stores nothing aside, so the release finds nothing
        /// there). The result is then not a proof (@c ProofLoss::MemoryPressure).
        ///
        /// @param search The search (a @c detail::Search).
        /// @param context    Its context.
        template <typename SearchType>
        void relieve_memory_pressure(SearchType& search,
                                     detail::SearchContext<ResourceType>& context) {
            const size_t limit = params_.memory_pressure_max_labels_per_node;
            quota_ = std::min(quota_, limit);
            if (memory_pressure_triggered_) {
                search.schedule.release_stored_aside(
                    context,
                    [&search](const LabelIterator<ResourceType>& position) {
                        search.erase(position);
                    });
            }
            memory_pressure_triggered_ = true;
            search.schedule.trim(context, limit);
        }

        /// @brief Release all label memory held by the pool and label containers.
        ///
        /// Called at the end of solve() so that RSS is reclaimed as soon as the
        /// caller returns.  The default implementation frees the pool (including
        /// shrink_to_fit).  Subclasses override this to also clear their own
        /// label pointer containers (non-dominated sets, unprocessed queues).
        virtual void release_label_memory() { label_pool_.release(); }

        // ── The hooks solve() calls ─────────────────────────────────────────
        //
        // Implemented by detail::LabellingAlgorithm (one search, forward or backward),
        // detail::Driven (a LabellingAlgorithm that a driver runs several times: it overrides
        // initialize, main_loop, prepare_next_run and record_solution),
        // detail::BidirectionalLabellingAlgorithm and DiversificationSearch. See solve() for the
        // order of the calls, and the overview at the top of labelling/search.hpp for a
        // labelling search.

        /// @brief Releases every label of the previous solve, then creates and queues one start
        ///        label per start node (the sources forward, the sinks backward, both
        ///        bidirectionally). DiversificationSearch creates none.
        virtual void initialize_labels() = 0;

        /// @brief How many labels wait in the schedule(s) to be taken. Labels stored aside are
        ///        not counted until @ref prepare_next_phase puts them back. solve() runs while it
        ///        is positive, and ends @c Complete only when it is 0 (DiversificationSearch
        ///        answers 1, so it never ends @c Complete).
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

        /// @brief Puts the labels stored aside (by the per-node quota or a memory-pressure trim)
        ///        back into the schedule, and starts the per-node counts again.
        ///
        /// Called by @ref prepare_next_run between two phases, and once more after the last run,
        /// so that labels still stored aside count in @ref number_of_waiting_labels when the
        /// status is set. Does nothing by default: a bidirectional search abandons labels
        /// instead of storing them aside.
        virtual void prepare_next_phase() {}

        /// @brief How many phases a solve may run: @c max_phases by default.
        ///
        /// A later phase extends the labels a quota or memory pressure stored aside. A search
        /// that abandons them instead runs one phase: a second would only restart the
        /// @c max_iterations count and record the paths at the end nodes again.
        [[nodiscard]] virtual size_t max_phases() const { return params_.max_phases; }

        /// @brief Prepares the next run of the solve's loop, and says whether there is one.
        ///
        /// The default starts the next phase while @ref max_phases allows it: the labels stored
        /// aside are extended in it. A driven search asks its driver instead (@c detail::Driven).
        ///
        /// @param number_of_runs How many runs the loop has made.
        /// @return Whether to run again.
        virtual bool prepare_next_run(size_t number_of_runs) {
            if (number_of_runs < max_phases()) {
                prepare_next_phase();
                return true;
            }
            return false;
        }

        /// @brief One run: takes labels (or, for Pulling, visits nodes) until none waits or a
        ///        stop applies (@ref should_end_loop, @c max_iterations, the memory limit,
        ///        @c max_solutions). solve() calls it once per phase, or once per run of a
        ///        driver.
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

        /// @brief The labels stored at the end nodes (the sinks forward, the sources backward);
        ///        @ref record_remaining_solutions records their paths.
        [[nodiscard]] virtual std::list<Label<ResourceType>*> labels_at_end_nodes() const = 0;

        /// @brief Logs every stored label, node by node, at the trace log level.
        virtual void print_labels() const {}

        /// @brief The arc ids of the path @p label holds, in path order (from the source).
        virtual std::vector<size_t> get_path_arc_ids(const Label<ResourceType>& label) = 0;

        /// @brief Records the path a forward label at an end node holds, from a source to the
        ///        label's node, unless it costs at least the upper bound (@ref record_path).
        ///
        /// A backward LabellingAlgorithm overrides it to call @ref record_backward_solution;
        /// Driven also notes the path as the end of its run.
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
        /// Used when no single label holds the path (e.g. joined pairs). Dropped here, for every
        /// caller: a path costing at least `upper_bound_`, an empty one, and one already recorded
        /// (`solutions_` hashes a solution by its arc ids).
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
        /// beginning of each solve and is lowered to
        /// @ref AlgorithmBaseParams::memory_pressure_max_labels_per_node by
        /// @ref relieve_memory_pressure. The schedules read it through
        /// @c SearchContext::quota, in place of @c max_extended_labels_per_node.
        size_t quota_ = kNoLimit;

        /// @brief True after on_memory_pressure() has been called at least once this solve.
        ///
        /// Read by @ref relieve_memory_pressure, which releases what is stored aside on every call
        /// but the first, and by solve(), which then records @c ProofLoss::MemoryPressure.
        bool memory_pressure_triggered_ = false;

        /// @brief The cost upper bound the caller gave this solve: a path is recorded only when
        ///        it costs less. Fixed for the solve.
        double upper_bound_ = std::numeric_limits<double>::infinity();
        /// @brief The incumbent: the cost of the cheapest path to an end node found so far in
        ///        this solve, or @ref upper_bound_ before any. It only falls; the incumbent prune
        ///        (@c prune_by_incumbent) and the join compare with it.
        double incumbent_ = std::numeric_limits<double>::infinity();
        std::unordered_set<Solution> solutions_;
        const ArcMask* arc_mask_ = nullptr;  ///< the hard arcs a solve leaves out; none by default
        bool graph_unchanged_ = false;       ///< see @ref set_graph_unchanged
        DominanceMask dominance_mask_;  ///< the relaxed rule's, built at the start of each solve

        /// @brief What this solve lost that its status cannot see; reset at @ref initialize,
        ///        which records a mask that is not empty.
        ProofRecord proof_;

        size_t number_of_extended_labels_ = 0;

        bool timed_out_ = false;
        bool last_solve_proven_ = false;      ///< Whether the last solve() was a proof.
        const Timer* solve_timer_ = nullptr;  ///< Points to solve()'s timer; null outside solve().
};

}  // namespace rcspp
