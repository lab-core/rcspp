// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <set>
#include <utility>

#include "rcspp/algorithm/limits.hpp"
#include "rcspp/algorithm/search_direction.hpp"
#include "rcspp/utils/memory.hpp"
#include "rcspp/validation/pre_solve_plan.hpp"

namespace rcspp {

// Forward declaration so AlgorithmBaseParams::with_container can name the return type.
template <typename LabelContainerType>
struct AlgorithmParams;

// ── Memory-limit algorithm defaults ──────────────────────────────────────────
/// Default fraction of RAM to use as the memory limit.
constexpr double kDefaultMemoryLimitFraction = 0.9;
/// Default number of main-loop iterations between RSS measurements.
constexpr size_t kDefaultMemoryCheckInterval = 50'000;
/// Default per-node queue size cap when memory pressure is triggered.
constexpr size_t kDefaultMemoryPressureMaxLabelsPerNode = 200;
/// Number of extensions (@c Algorithm::count_extension) between two debug progress reports.
constexpr size_t kProgressReportInterval = 100'000;

struct AlgorithmBaseParams {
        void check() const {  // NOLINT(readability-make-member-function-const)
            if (max_phases > 1 && max_extended_labels_per_node >= kNoLimit) {
                LOG_WARN(
                    "AlgorithmParams: max_phases > 1 with no per-node quota "
                    "(max_extended_labels_per_node is unlimited): a further phase runs only when "
                    "memory pressure stores labels aside, or when a bidirectional search stops at "
                    "max_iterations.\n");
            }
            if (direction == SearchDirection::Bidirectional && max_phases > 1) {
                LOG_WARN(
                    "AlgorithmParams: max_phases > 1 has no effect on a bidirectional search, "
                    "which runs one phase.\n");
            }
            if (return_dominated_solutions && max_solutions >= kNoLimit) {
                LOG_WARN(
                    "AlgorithmParams: max_solutions == MAX and return_dominated_solutions "
                    "is set to true. return_dominated_solutions will not have any effects, set "
                    "max_solutions to a lower value.\n");
            }
        }

        [[nodiscard]] bool could_be_non_optimal() const {
            return ((max_solutions < kNoLimit) || (max_extended_labels_per_node < kNoLimit) ||
                    std::isfinite(timeout_s));
        }

        // stop after finding this many solutions (not going to optimality)
        size_t max_solutions = kNoLimit;

        // whether to also return dominated solutions found at the sink nodes
        bool return_dominated_solutions = false;

        /// @brief Whether a search drops a label that cannot lead to a path cheaper than the
        ///        incumbent (the cheapest path found so far in this solve, or the solve's upper
        ///        bound until one is found): its cost plus its completion bound is at or above the
        ///        incumbent (default false).
        ///
        /// Exact under an additive cost: one Bellman–Ford run per solve computes the bound. Under
        /// another cost, the label's cost alone is compared, which may drop the cheapest path when
        /// arcs have negative costs. (Pushing and Pulling, not yet on the labelling kernel, still
        /// compare the cost alone.)
        bool prune_by_incumbent = false;

        /// @brief The per-node quota: at most this many labels are taken at each node in each
        ///        phase (no limit by default, @ref kNoLimit).
        ///
        /// A non-dominated label over its node's quota is not lost: a single-direction search
        /// stores it aside and extends it in the next phase (@ref max_phases). A bidirectional
        /// search, which runs one phase, abandons it: it stays in its node's container, never
        /// extended. Labels still stored aside, or abandoned, when the solve ends make it
        /// @c AlgorithmStatus::Truncated. Memory pressure may lower the quota
        /// (@ref memory_pressure_max_labels_per_node).
        size_t max_extended_labels_per_node = kNoLimit;

        /// @brief How many phases a solve may run (default 1).
        ///
        /// A phase extends labels until none waits or a stop applies. When labels were stored
        /// aside (by the per-node quota or a memory-pressure trim), the next phase puts them
        /// back and extends them, with the per-node counts started again. Labels still stored
        /// aside after the last phase make the solve @c AlgorithmStatus::Truncated. A
        /// bidirectional search runs one.
        size_t max_phases = 1;

        // maximum number of iterations/loops (for algorithms that use it)
        size_t max_iterations = kNoLimit;

        // wall-clock timeout in seconds; solve() returns early when elapsed >= timeout_s
        double timeout_s = std::numeric_limits<double>::infinity();

        // callable returning true if the algorithm should stop early (e.g. SIGINT)
        std::function<bool()> should_stop;

        // numerical tolerance used for cost comparisons
        double tolerance = 1e-9;  // NOLINT(readability-magic-numbers)

        /// @brief Which way the search runs. Forward by default.
        ///
        /// Backward and Bidirectional need the model's backward semantics, which are checked
        /// before the solve; an algorithm that does not support the direction throws
        /// std::invalid_argument when it is created or solves.
        SearchDirection direction = SearchDirection::Forward;

        /// @brief If true (default), release all label memory at the end of solve().
        ///
        /// Set to false when the same algorithm instance is called repeatedly in a
        /// tight loop (e.g., inside DiversificationSearch) so the pool capacity is
        /// retained across calls and shrink_to_fit() overhead is avoided.
        bool release_after_solve = true;

        // for tabu search algorithms
        size_t tabu_tenure = 5;  // NOLINT(readability-magic-numbers)
        std::set<size_t> tabu_exempt_node_ids;
        bool tabu_random_noise = true;

        /// @brief Consecutive non-improving dives before a diversification step.
        ///
        /// Used by @ref ImprovingTabuSearch: when this many dives in a row fail
        /// to strictly improve the best known cost, @c grow_extra() is called on
        /// the tabu list to force exploration of unexplored regions.
        size_t non_improving_episodes_before_growth = 10;  // NOLINT(readability-magic-numbers)

        int seed = 0;

        /// @brief Index of the critical resource: the "clock" on which the bidirectional search
        ///        sets its half-way point.
        ///
        /// The clock must never decrease along an arc (e.g. time or load). The index counts only
        /// the components of the clock's resource type, which is a template parameter of the
        /// algorithm.
        size_t critical_resource_index = 0;

        /// @brief Half-way point `H` on the critical resource; **0 turns the bound off**.
        ///
        /// Forward labels stop above `H`, backward labels below it. Set it to about half the
        /// clock's range. With 0, both searches run unbounded: correct but slower.
        /// @c BidirectionalStats::half_way_used, in @c SolveResult::bidirectional, reports
        /// which happened.
        double half_way_point = 0.0;

        /// @brief Whether the join still runs after a timeout or the memory limit (default true).
        ///
        /// Bidirectional only. With the half-way point on, few forward labels reach a sink,
        /// so an early stop without the join returns almost nothing. With this set, the join
        /// still runs and keeps only paths cheaper than the best one found so far. An interrupt
        /// (`should_stop`) always skips the join. Set false to skip the join after a timeout or
        /// the memory limit as well.
        bool join_after_early_stop = true;

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

        /// @brief How many waiting labels a memory-pressure trim keeps, per node (default 200).
        ///
        /// Each time a memory check finds the pressure threshold reached
        /// (@ref memory_pressure_fraction), the search trims the labels waiting to be extended.
        /// `Pushing` and `Pulling` keep this many at each node; `Simple` and `AStar` keep this
        /// many times the number of nodes in all, the best (non-dominated, then cheapest; A* by
        /// its key). Of the labels trimmed, a dominated one is released, and a non-dominated one
        /// is stored aside for the next phase, or abandoned on a side of a bidirectional search.
        /// From the second trim of a solve on, every label stored aside is released first, to
        /// free memory. The per-node quota (@ref max_extended_labels_per_node) is also lowered to
        /// this value for the rest of the solve. The result may then not be optimal
        /// (@c Algorithm::memory_pressure_was_triggered).
        size_t memory_pressure_max_labels_per_node = kDefaultMemoryPressureMaxLabelsPerNode;

        /// @brief Wrap these base params in an AlgorithmParams with the given container.
        ///
        /// @param container Label container instance (e.g. LabelList or LabelBuckets).
        ///                  Defaults to a default-constructed LabelContainerType{} when
        ///                  LabelContainerType is default-constructible (e.g. LabelList).
        /// @return AlgorithmParams<LabelContainerType> inheriting all settings from *this.
        template <typename LabelContainerType>
        AlgorithmParams<LabelContainerType> with_container(
            LabelContainerType container = LabelContainerType{}) const;
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
template <typename LabelContainerType>
AlgorithmParams<LabelContainerType> AlgorithmBaseParams::with_container(
    LabelContainerType container) const {
    return AlgorithmParams<LabelContainerType>(*this, std::move(container));
}

}  // namespace rcspp
