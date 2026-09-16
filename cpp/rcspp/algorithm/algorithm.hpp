// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <concepts>  // NOLINT(build/include_order)
#include <functional>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "rcspp/algorithm/label_buckets.hpp"
#include "rcspp/algorithm/solution.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label_pool.hpp"
#include "rcspp/resource/concrete/numerical_resource.hpp"
#include "rcspp/utils/memory.hpp"
#include "rcspp/utils/timer.hpp"

namespace rcspp {

/// @brief Exit status returned by Algorithm::solve().
enum class AlgorithmStatus {
    COMPLETE,       ///< All labels processed; result is optimal (given other params).
    TIMEOUT,        ///< Wall-clock timeout reached before completion.
    MAX_SOLUTIONS,  ///< stop_after_X_solutions reached.
    MAX_PHASES,     ///< num_max_phases exhausted with labels still remaining.
    INTERRUPTED,    ///< External should_stop callback returned true.
    MEMORY_LIMIT,   ///< RSS memory limit reached.
};

/// @brief Human-readable name for an @ref AlgorithmStatus value.
///
/// Note: only @ref AlgorithmStatus::COMPLETE means the search was exhaustive; every other
/// status indicates the solve was cut short (so e.g. "no solution found" does not prove that
/// none exists).
[[nodiscard]] inline std::string to_string(AlgorithmStatus status) {
    switch (status) {
        case AlgorithmStatus::COMPLETE:
            return "complete";
        case AlgorithmStatus::TIMEOUT:
            return "timeout";
        case AlgorithmStatus::MAX_SOLUTIONS:
            return "max_solutions";
        case AlgorithmStatus::MAX_PHASES:
            return "max_phases";
        case AlgorithmStatus::INTERRUPTED:
            return "interrupted";
        case AlgorithmStatus::MEMORY_LIMIT:
            return "memory_limit";
    }
    return "unknown";
}

/// @brief Return value of Algorithm::solve().
struct SolveResult {
        std::vector<Solution> solutions;
        AlgorithmStatus status = AlgorithmStatus::COMPLETE;

        /// @brief Whether a directional bound was in force for this solve.
        ///
        /// Bidirectional only; `false` from every other algorithm, which have no such bound. A
        /// bidirectional solve whose clock fails validation runs *both* searches to completion and
        /// then joins every pair, which is strictly more work than a forward search alone -- so a
        /// disabled bound is not merely an un-accelerated solve, it is a pessimisation. It is
        /// reported here rather than only logged because the caller who needs to know is usually a
        /// Python caller, who never holds the algorithm object.
        bool bounded_by_half_way = false;

        /// @brief How many complete paths the join pass produced.
        ///
        /// Bidirectional only; `0` from every other algorithm. The one number that tells the two
        /// payoff failures apart. Zero on an instance that still returns the optimum means the
        /// answer came from a search reaching a terminal rather than from the join -- which is what
        /// happens to a route whose clock never reaches `H`. A number that dwarfs the label count
        /// means the opposite: the crossing test is not filtering.
        size_t number_of_joined_paths = 0;

        /// @brief Whether memory pressure trimmed this solve, so the result may not be optimal.
        ///
        /// Reported by every algorithm, not only the bidirectional one. A pressure event trims
        /// the unprocessed queues and tightens the per-node extension quota: labels that were
        /// never extended are abandoned, and the answer stops being a proof.
        ///
        /// **This is the only lossiness the status cannot express.** @c AlgorithmStatus::COMPLETE
        /// means "the label sets were exhausted", which stays true after a trim -- they were
        /// exhausted *of what survived it*. There is no status value for "exhausted but lossy",
        /// and reusing @c MEMORY_LIMIT would conflate a hard stop with a soft trim, so it is a
        /// flag rather than a status. A caller treating @c COMPLETE as a proof of optimality --
        /// a column-generation loop deciding it has converged, say -- has to read this too.
        ///
        /// @c Algorithm::memory_pressure_was_triggered() is the same value, and is the one a C++
        /// caller holding the algorithm object can reach. This is the copy that crosses the
        /// Python boundary, where the algorithm object does not.
        bool memory_pressure_triggered = false;

        /// @brief Human-readable name of the exit status.
        [[nodiscard]] std::string status_string() const { return to_string(status); }
};

// Forward declaration so AlgorithmBaseParams::with_container can name the return type.
template <typename LabelContainerType>
struct AlgorithmParams;

template <typename ResourceType>
using LabelIterator = std::list<Label<ResourceType>*>::iterator;

template <typename ResourceType>
using LabelIteratorPair =
    std::pair<Label<ResourceType>*, typename std::list<Label<ResourceType>*>::iterator>;

constexpr size_t MAX_INT = std::numeric_limits<int>::max() / 2;  // to avoid overflow

// ── Memory-limit algorithm defaults ──────────────────────────────────────────
/// Default fraction of RAM to use as the memory limit.
constexpr double kDefaultMemoryLimitFraction = 0.9;
/// Default number of main-loop iterations between RSS measurements.
constexpr size_t kDefaultMemoryCheckInterval = 50'000;
/// Default per-node queue size cap when memory pressure is triggered.
constexpr size_t kDefaultMemoryPressureMaxLabelsPerNode = 200;

struct AlgorithmBaseParams {
        void check() const {  // NOLINT(readability-make-member-function-const)
            if (num_max_phases > 1 && num_labels_to_extend_by_node >= MAX_INT) {
                LOG_WARN(
                    "AlgorithmParams: num_labels_to_extend_by_node == MAX and num_max_phases > 1. "
                    "num_max_phases will not have any effects, set num_labels_to_extend_by_node to "
                    "a lower value.\n");
            }
            if (num_max_phases > 1 && stop_after_X_solutions >= MAX_INT) {
                LOG_WARN(
                    "AlgorithmParams: stop_after_X_solutions == MAX and num_max_phases > 1. "
                    "num_max_phases will not have any effects, set stop_after_X_solutions to a "
                    "lower value.\n");
            }
            if (return_dominated_solutions && stop_after_X_solutions >= MAX_INT) {
                LOG_WARN(
                    "AlgorithmParams: stop_after_X_solutions == MAX and return_dominated_solutions "
                    "is set to true. return_dominated_solutions will not have any effects, set "
                    "stop_after_X_solutions to a lower value.\n");
            }
        }

        [[nodiscard]] bool could_be_non_optimal() const {
            return ((stop_after_X_solutions < MAX_INT) ||
                    (num_labels_to_extend_by_node < MAX_INT) || std::isfinite(timeout_s));
        }

        // stop after finding X solutions (not going to optimality)
        size_t stop_after_X_solutions = MAX_INT;

        // whether to also return dominated solutions found at the sink nodes
        bool return_dominated_solutions = false;

        // if true, prune label if greater than the best upper bound
        bool prune_based_on_upper_bound_ = false;

        // for using label pool (should normally always be true)
        bool use_pool = true;

        // for truncated labeling
        size_t num_labels_to_extend_by_node = MAX_INT;

        // maximum number of passes for the resolution if previous pass ended early with not enough
        // solutions
        size_t num_max_phases = 1;

        // maximum number of iterations/loops (for algorithms that use it)
        size_t max_iterations = MAX_INT;

        // wall-clock timeout in seconds; solve() returns early when elapsed >= timeout_s
        double timeout_s = std::numeric_limits<double>::infinity();

        // callable returning true if the algorithm should stop early (e.g. SIGINT)
        std::function<bool()> should_stop;

        // numerical tolerance used for cost comparisons
        double tolerance = 1e-9;  // NOLINT(readability-magic-numbers)

        /// @brief If true (default), release all label memory at the end of solve().
        ///
        /// Set to false when the same algorithm instance is called repeatedly in a
        /// tight loop (e.g., inside DiversificationSearch) so the pool capacity is
        /// retained across calls and shrink_to_fit() overhead is avoided.
        bool release_after_solve = true;

        // for tabu search algorithms
        size_t tabu_tenure = 5;  // NOLINT(readability-magic-numbers)
        std::set<size_t> forbidden_tabu;
        bool tabu_random_noise = true;

        /// @brief Consecutive non-improving dives before a diversification step.
        ///
        /// Used by @ref ImprovingTabuSearch: when this many dives in a row fail
        /// to strictly improve the best known cost, @c grow_extra() is called on
        /// the tabu list to force exploration of unexplored regions.
        size_t diversification_tenure = 10;  // NOLINT(readability-magic-numbers)

        int seed = 0;

        /// @brief Index of the cost resource component that cost-to-go bounds relax on.
        ///
        /// Injected by the dispatch layer (see AStarAlgoEntry and BidirectionalAlgoEntry in
        /// graph_impl.hpp) so that a cost-to-go bound runs Bellman–Ford on the same cost slot as
        /// the labeling algorithm itself. Read by @c AStarDominanceAlgorithm (its heuristic) and
        /// by @c BidirectionalDominanceAlgorithm (its completion bounds); ignored by every other
        /// algorithm.
        ///
        /// The name is historical -- this was an A*-only field. It is not renamed because it is
        /// public C++ API, and a second field meaning the same thing is exactly the
        /// two-sources-of-truth hazard the rest of this design refuses.
        size_t heuristic_cost_index = 0;

        /// @brief Component index of the monotone bounding resource used as the bidirectional
        ///        clock.
        ///
        /// Injected by the dispatch layer, like @ref heuristic_cost_index. The resource *type*
        /// is a template parameter of the algorithm; this is the index within that type's slot.
        size_t critical_resource_index = 0;

        /// @brief Half-way point `H` on the critical resource; **0 turns the bound off**.
        ///
        /// Each direction stops at `H`: the forward search discards labels whose clock exceeds
        /// it, the backward search discards labels whose clock falls below it, and the join pairs
        /// what is left. Set it to roughly half the clock's range -- the algorithm takes that
        /// range to be `[0, 2H]`, so `H` is the only number it has to go on.
        ///
        /// **Zero does not derive anything.** `HalfWayPolicy` does have a "derive `H` as `R / 2`"
        /// branch, but it needs a finite `R`, and there is no general accessor for a feasibility
        /// function's upper bound -- so `BidirectionalDominanceAlgorithm::resource_upper_bound()`
        /// reports `2H` when `H` is given and infinity when it is not. Zero therefore reaches
        /// `HalfWayPolicy` as "no `H` and no finite `R`", which starts the bound disabled. The
        /// derive branch is reachable, and tested, only from a direct construction with a known
        /// `R`.
        ///
        /// That makes zero the supported way to ask for **two unbounded searches plus a join**,
        /// and the equivalence suite relies on it for exactly that. A disabled bound costs speed,
        /// not optimality: both searches run to completion, the join considers every pair, and
        /// the answer is the same one a forward search would give -- for strictly more work than
        /// a forward search would do. @c SolveResult::bounded_by_half_way reports which of the
        /// two happened, so it is worth checking rather than assuming.
        ///
        /// So: a bidirectional solve that wants the speed-up sets this explicitly.
        double half_way_point = 0.0;

        /// @brief Reserved for a half-way policy that moves as the search runs. Not yet read.
        ///
        /// `HalfWayPolicy` is an object rather than a pair of numbers precisely so a dynamic
        /// variant can replace the static one without re-plumbing the algorithm, and this is the
        /// switch that variant will use. Setting it today changes nothing, and
        /// `BidirectionalDominanceAlgorithm::configure_half_way` says so at WARN level rather than
        /// letting a caller conclude the policy is broken.
        ///
        /// Deliberately **not** exposed to Python: a C++ caller can read this comment and a Python
        /// caller cannot, so there the flag would only mislead.
        bool dynamic_half_way = false;

        // ── Memory-limit parameters ─────────────────────────────────────────

        /// @brief Hard upper bound on process RSS in gibibytes (GiB); 0 means unlimited.
        ///
        /// When non-zero, the algorithm stops (returning whatever solutions have
        /// been found so far) as soon as the measured RSS reaches this value.
        /// Fractional values are accepted (e.g. 0.5 for 512 MiB, 1.5 for 1.5 GiB).
        ///
        /// This takes priority over @ref limit_to_available_ram and
        /// @ref limit_to_total_ram; both of those are ignored when this is set.
        ///
        /// @code
        ///   params.max_memory_gb = 8.0;   // 8 GiB hard limit
        ///   params.max_memory_gb = 0.5;   // 512 MiB hard limit
        /// @endcode
        double max_memory_gb = 0.0;

        /// @brief Automatically derive the memory limit from currently *available* RAM.
        ///
        /// If true, the limit is set to
        /// @ref memory_limit_fraction × available-system-RAM at the start of
        /// solve().  "Available" means memory the OS can give out without
        /// swapping — it fluctuates as other processes allocate and free memory.
        ///
        /// Ignored when @ref max_memory_gb is non-zero.
        /// Takes priority over @ref limit_to_total_ram.
        bool limit_to_available_ram = false;

        /// @brief Automatically derive the memory limit from the machine's *total* RAM.
        ///
        /// If true, the limit is set to
        /// @ref memory_limit_fraction × total-physical-RAM at the start of
        /// solve().  Unlike @ref limit_to_available_ram, total RAM is a fixed
        /// hardware constant (e.g. 16 GiB) and does not change between solves.
        /// This gives a stable, reproducible limit regardless of what other
        /// processes are doing.
        ///
        /// Ignored when @ref max_memory_gb is non-zero or when
        /// @ref limit_to_available_ram is true.
        bool limit_to_total_ram = false;

        /// @brief Fraction of RAM to use when @ref limit_to_available_ram or
        ///        @ref limit_to_total_ram is true.
        ///
        /// Must be in (0, 1].  Default 0.9 leaves a 10 % safety margin.
        double memory_limit_fraction = kDefaultMemoryLimitFraction;

        /// @brief Number of main-loop iterations between consecutive RSS checks.
        ///
        /// Larger values reduce measurement overhead; smaller values react
        /// faster to memory spikes.  Must be > 0.
        size_t memory_check_interval = kDefaultMemoryCheckInterval;

        /// @brief Pressure threshold as a fraction of the effective memory limit.
        ///
        /// When `current_rss >= memory_pressure_fraction × effective_limit`,
        /// @ref on_memory_pressure() is called to prune label queues before
        /// growth reaches the hard limit.  Default 0.8 triggers at 80 %.
        double memory_pressure_fraction = kDefaultMemoryPressureFraction;

        /// @brief Max unprocessed labels to retain per node under memory pressure.
        ///
        /// When @ref on_memory_pressure() fires, each per-node unprocessed
        /// queue is trimmed to this many entries (cheapest labels kept).
        /// Dominated excess labels are recycled; non-dominated excess labels
        /// are stored for a future phase, consistent with truncated labeling.
        size_t memory_pressure_max_labels_per_node = kDefaultMemoryPressureMaxLabelsPerNode;

        /// @brief Wrap these base params in an AlgorithmParams with the given container.
        ///
        /// @param container Label container instance (e.g. LabelList or LabelBuckets).
        ///                  Defaults to a default-constructed LC{} when LC is
        ///                  default-constructible (e.g. LabelList).
        /// @return AlgorithmParams<LC> inheriting all settings from *this.
        template <typename LC>
        AlgorithmParams<LC> with_container(LC container = LC{}) const;
};

template <typename LabelContainerType>
struct AlgorithmParams : AlgorithmBaseParams {
        explicit AlgorithmParams(LabelContainerType labels = LabelContainerType())
            : AlgorithmBaseParams(), labels(std::move(labels)) {}

        explicit AlgorithmParams(AlgorithmBaseParams base_params,
                                 LabelContainerType labels = LabelContainerType())
            : AlgorithmBaseParams(std::move(base_params)), labels(std::move(labels)) {}

        // Container to store labels, could be overridden with Buckets
        const LabelContainerType labels;
};

// Out-of-line definition: AlgorithmParams is now complete.
template <typename LC>
AlgorithmParams<LC> AlgorithmBaseParams::with_container(LC container) const {
    return AlgorithmParams<LC>(*this, std::move(container));
}

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
         * @brief Checks whether the algorithm has reached an optimal state.
         *
         * An algorithm is considered optimal when there are no more labels left to process,
         * i.e., when @ref number_of_labels returns zero. This typically means that all possible
         * extensions have been explored and no further improvements or solutions can be found.
         *
         * The default implementation returns true if @ref number_of_labels() == 0.
         * Derived classes may override this method to provide a more specific notion of optimality.
         *
         * @return true if the algorithm is optimal (no labels left to process), false otherwise.
         */
        [[nodiscard]] virtual bool is_optimal() const { return number_of_labels() == 0; }

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
            cost_upper_bound_ = cost_upper_bound;
            best_cost_upper_bound_ = cost_upper_bound;
            label_pool_.clear();
            solutions_.clear();
            effective_max_labels_per_node_ = params_.num_labels_to_extend_by_node;
            memory_pressure_triggered_ = false;
        }

        virtual SolveResult solve(const Graph<ResourceType>* graph, double cost_upper_bound) {
            // initialization
            Timer timer(true);
            timed_out_ = false;
            solve_timer_ = &timer;
            memory_limit_.resolve(params_.max_memory_gb,
                                  params_.limit_to_available_ram,
                                  params_.limit_to_total_ram,
                                  params_.memory_limit_fraction,
                                  params_.memory_pressure_fraction);
            initialize(graph, cost_upper_bound);

            // initialize labels
            this->initialize_labels();

            size_t num_phases = 0;
            while (!should_stop() && number_of_labels() > 0) {
                // main labeling loop
                main_loop();

                // extract solutions any remaining solutions
                extract_remaining_solutions();

                // prepare next phase (if any)
                if (++num_phases < params_.num_max_phases) {
                    prepareNextPhase();
                } else {
                    break;
                }
            }

            solve_timer_ = nullptr;

            if (LOG_DEBUG_ACTIVE()) {
                LOG_DEBUG("Total number of extended labels: ", num_extended_labels_, "\n");
                print_labels();
            }

            // recover solutions
            std::vector<Solution> solutions;
            solutions.reserve(solutions_.size());
            for (auto&& solution : solutions_) {
                solutions.push_back(std::move(solution));
            }

            // prepare next phase to ensure that all_labels_processed() returns the right value
            // (if not releasing all labels). Also, next solve() is ready to start if needed
            if (!params_.release_after_solve) {
                prepareNextPhase();
            }

            // sort solutions
            std::ranges::sort(solutions,
                              [](const Solution& a, const Solution& b) { return a.cost < b.cost; });

            LOG_DEBUG("Number of solutions before resize: ", solutions.size(), '\n');
            LOG_DEBUG("Min cost=",
                      solutions.empty() ? cost_upper_bound : solutions.front().cost,
                      "\n");
            LOG_DEBUG("Total time=", timer.elapsed_seconds(), " sec.\n");

            // resize solutions if needed
            if (solutions.size() > params_.stop_after_X_solutions) {
                solutions.resize(params_.stop_after_X_solutions);
            }

            // Determine the exit status. "All labels processed" (number_of_labels() == 0) means
            // the search finished and the result is optimal, so it takes precedence over the
            // early-stop reasons below: a timeout / interrupt / memory flag must NOT downgrade a
            // finished, optimal run. This matters because is_interrupted() and
            // memory_limit_.is_exceeded() are re-evaluated here — the external stop callback may
            // have flipped, or RSS may exceed the limit because of the retained solutions — even
            // though the loop actually exited by exhausting all labels. The remaining branches
            // describe why the search stopped *early* (labels still remain).
            AlgorithmStatus status;
            if (number_of_labels() == 0) {
                status = AlgorithmStatus::COMPLETE;
            } else if (timed_out_) {
                status = AlgorithmStatus::TIMEOUT;
            } else if (is_interrupted()) {
                status = AlgorithmStatus::INTERRUPTED;
            } else if (memory_limit_.is_exceeded()) {
                status = AlgorithmStatus::MEMORY_LIMIT;
            } else if (solutions.size() >= params_.stop_after_X_solutions) {
                status = AlgorithmStatus::MAX_SOLUTIONS;
            } else {
                status = AlgorithmStatus::MAX_PHASES;
            }

            // Optionally release label memory so RAM is reclaimed when the caller returns.
            if (params_.release_after_solve) {
                release_label_memory();
            }

            SolveResult result{.solutions = std::move(solutions), .status = status};
            annotate(&result);
            return result;
        }

        [[nodiscard]] bool all_labels_processed() const { return number_of_labels() == 0; }

        [[nodiscard]] bool is_interrupted() const {
            return params_.should_stop && params_.should_stop();
        }

        /// @brief Read-only access to the label pool, for diagnostics and tests.
        [[nodiscard]] const LabelPool<ResourceType>& get_label_pool() const { return label_pool_; }

        /// @brief How many labels the last solve extended.
        ///
        /// The measurement that says whether a search strategy is paying off. Label *extensions*
        /// are what the work actually is, and they are comparable across algorithms in a way that
        /// wall-clock time is not: the same instance on a busier machine takes longer without any
        /// algorithm having changed. Read-only, and already counted -- only the accessor is new.
        ///
        /// @return The number of label extensions performed.
        [[nodiscard]] size_t get_number_of_extended_labels() const { return num_extended_labels_; }

        /// @brief Whether memory pressure fired at least once during the last solve.
        ///
        /// A pressure event trims queues and tightens the per-node extension quota, so the result
        /// may no longer be optimal. `could_be_non_optimal()` cannot report it: that predicate
        /// reads `params_` only, and memory pressure is a property of the run rather than of the
        /// request. This is the accessor that closes the gap -- the same role
        /// `bounded_by_half_way()` plays for the half-way bound.
        ///
        /// @return `true` when `on_memory_pressure()` was called during the last solve.
        [[nodiscard]] bool memory_pressure_was_triggered() const {
            return memory_pressure_triggered_;
        }

    protected:
        bool print_{false};

        /// @brief Adds diagnostics to the result, just before it is returned.
        ///
        /// `SolveResult` is what crosses the Python boundary; the algorithm object does not. So a
        /// diagnostic a user is told to check has to arrive here.
        ///
        /// The base reports the one diagnostic every algorithm has: whether memory pressure
        /// trimmed the run, which no @ref AlgorithmStatus value can express. **An override must
        /// call this** -- `Base::annotate(result)` -- or its algorithm silently stops reporting
        /// it.
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

        /// @brief Release all label memory held by the pool and label containers.
        ///
        /// Called at the end of solve() so that RSS is reclaimed as soon as the
        /// caller returns.  The default implementation frees the pool (including
        /// shrink_to_fit).  Subclasses override this to also clear their own
        /// label pointer containers (non-dominated sets, unprocessed queues).
        virtual void release_label_memory() { label_pool_.release(); }

        // ── Core virtuals ───────────────────────────────────────────────────

        virtual void initialize_labels() = 0;

        [[nodiscard]] virtual size_t number_of_labels() const = 0;

        virtual void prepareNextPhase() {}

        virtual void main_loop() = 0;

        /// @brief Records the solutions still sitting at terminal nodes when the loop ends.
        ///
        /// Virtual because a bidirectional search has more than one source of complete paths:
        /// forward labels at sinks, backward labels at sources, and joined pairs.
        virtual void extract_remaining_solutions() {
            auto labels_at_sinks = this->get_labels_at_sinks();
            for (const auto* sink_label : labels_at_sinks) {
                this->extract_solution(*sink_label);
            }
        }

        [[nodiscard]] virtual std::list<Label<ResourceType>*> get_labels_at_sinks() const = 0;

        virtual void print_labels() const {}

        virtual std::string path_to_string(const Label<ResourceType>& label) {
            auto path = get_path_arc_ids(label);
            std::stringstream ss;
            for (const size_t arc_id : path) {
                ss << graph_->get_arc(arc_id)->destination->id << " ";
            }
            return ss.str();
        }

        virtual std::vector<size_t> get_path_arc_ids(const Label<ResourceType>& label) = 0;

        virtual void extract_solution(const Label<ResourceType>& end_label) {
            if (end_label.get_cost() >= cost_upper_bound_) {
                return;
            }

            extract_solution(end_label.get_cost(),
                             this->get_path_arc_ids(end_label),
                             end_label.get_end_node()->id);
        }

        /// @brief Build and record a Solution from an explicit path.
        ///
        /// Used by the bidirectional algorithm, where a complete path can come from a joined pair
        /// or from a backward label reaching a source, and no single label holds the merged cost
        /// or the end node.
        ///
        /// Applies the caller's `cost_upper_bound_` here rather than at each call site: the
        /// `Label` overload above already filters, and the joiner's own cutoff already implies
        /// this test, but `extract_backward_solution` had no filter of its own and returned
        /// columns above the bound. One guard on the one function every solution passes through is
        /// the version that cannot be forgotten by the next caller.
        ///
        /// @param cost         Total cost of the path.
        /// @param path_arc_ids Arc ids of the path, in traversal order.
        /// @param end_node_id  Id of the node the path ends at.
        virtual void extract_solution(double cost, std::vector<size_t> path_arc_ids,
                                      size_t end_node_id) {
            if (cost >= cost_upper_bound_) {
                return;
            }
            if (path_arc_ids.empty()) {
                return;
            }

            // Build column: sum original arc costs and aggregate constraint coefficients
            Column column;
            std::unordered_map<size_t, long double> row_map;
            std::vector<size_t> path_node_ids;
            path_node_ids.reserve(path_arc_ids.size() + 1);
            for (size_t arc_id : path_arc_ids) {
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

            auto sol = Solution(cost,
                                std::move(path_node_ids),
                                std::move(path_arc_ids),
                                std::move(column));

            // solution already extracted
            if (solutions_.contains(sol)) {
                return;
            }

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

        /// @brief Returns true when solve() should terminate the outer loop.
        ///
        /// Checks (in order): iteration budget, solution budget, timeout, and the
        /// external stop callback.  Pass iteration=0 to skip the iteration check.
        ///
        /// @param iteration Current iteration index (default 0 = no iteration limit check).
        bool should_stop(size_t iteration = 0) {
            return iteration >= params_.max_iterations ||
                   solutions_.size() >= params_.stop_after_X_solutions || is_time_out() ||
                   is_interrupted();
        }

        LabelPool<ResourceType> label_pool_;
        const Graph<ResourceType>* graph_;
        const AlgorithmParams<LabelContainerType> params_;
        MemoryLimitHelper memory_limit_;  ///< Resolved at the start of each solve().

        /// @brief Effective per-node extension cap.
        ///
        /// Starts at @ref AlgorithmBaseParams::num_labels_to_extend_by_node at the
        /// beginning of each solve and is tightened to
        /// @ref AlgorithmBaseParams::memory_pressure_max_labels_per_node when
        /// @ref on_memory_pressure() fires.  Subclasses use this instead of
        /// @c params_.num_labels_to_extend_by_node wherever the per-node limit is enforced.
        size_t effective_max_labels_per_node_ = MAX_INT;

        /// @brief True after on_memory_pressure() has been called at least once this solve.
        ///
        /// Used by subclasses to distinguish a first pressure event (prune & store aside)
        /// from subsequent ones (prune & discard stored labels to free more memory).
        bool memory_pressure_triggered_ = false;

        double cost_upper_bound_ = std::numeric_limits<double>::infinity();
        double best_cost_upper_bound_ = std::numeric_limits<double>::infinity();
        std::unordered_set<Solution> solutions_;

        size_t nb_dominated_labels_{0};
        size_t num_extended_labels_ = 0;
        Timer total_full_extend_time_;

        bool timed_out_ = false;
        const Timer* solve_timer_ = nullptr;  ///< Points to solve()'s timer; null outside solve().
};
}  // namespace rcspp
