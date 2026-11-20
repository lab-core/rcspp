// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <set>
#include <utility>

#include "rcspp/algorithm/labelling/parts/admission/admission.hpp"
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
        /// @brief Logs a warning for each setting that has no effect or contradicts another;
        ///        never throws. Called by the Algorithm constructor. The errors that make a solve
        ///        throw (@c max_admitted_labels_per_node 0, an unknown component in
        ///        @c dominance_ignored_components) are found when the solve starts.
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
            check_admission();
        }

        /// @brief True if these params alone may keep a solve from being a proof: a limit on the
        ///        solutions or on the labels per node, a time limit, or an admission rule that is
        ///        not exact.
        ///
        /// A false answer does not promise a proof. An interrupt, the memory limit or a finite
        /// @c max_iterations can still stop the solve early, and the incumbent prune or an arc
        /// mask can still lose the proof. Read @c SolveResult::proven after the solve.
        [[nodiscard]] bool could_be_non_optimal() const {
            return ((max_solutions < kNoLimit) || (max_extended_labels_per_node < kNoLimit) ||
                    std::isfinite(timeout_s) || is_admission_inexact());
        }

        /// @brief Whether the admission rule may drop a label exact dominance keeps: `Relaxed`
        ///        with a component left out, or `KBest` with a limit.
        [[nodiscard]] bool is_admission_inexact() const {
            return (admission_rule == parts::AdmissionRule::Relaxed &&
                    !dominance_ignored_components.empty()) ||
                   (admission_rule == parts::AdmissionRule::KBest &&
                    max_admitted_labels_per_node < kNoLimit);
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
        /// Exact when the completion bound is valid: one Bellman–Ford run per solve computes it. It
        /// is valid when the cost is additive (@c Resource::is_cost_additive, false under a
        /// declared clamp such as @c AdditionExtensionFunction's floor) and no arc adds less than
        /// its share of the cost, whatever the label it extends, which the solve tests at the start
        /// by running the extension on the model's arcs (an undeclared floor or ceiling, or a
        /// maximum, in a priced extension fails it). Otherwise the label's cost alone is compared,
        /// which may drop the cheapest path when an arc lowers the cost; the result is then not a
        /// proof (@c ProofLoss::IncumbentPrune). See @ref prune_by_threshold for a prune against
        /// the upper bound alone.
        bool prune_by_incumbent = false;

        /// @brief Whether a labelling search drops a label that cannot complete below the solve's
        ///        upper bound (default false).
        ///
        /// When a label is taken, its cost plus a lower bound on the cost still to pay from its
        /// node is compared with the upper bound the solve was given: at or above it, no path
        /// through the label could be returned, and the label is dropped. The bound is a
        /// Bellman–Ford run over the arcs' shares of the cost, from the end nodes, at the start of
        /// a solve (kept from the previous solve when the graph is promised unchanged,
        /// @c Algorithm::set_graph_unchanged); a margin of `tolerance × max(1, |upper bound|)`
        /// keeps its rounding from dropping a path that would be returned. The returned paths are
        /// those of the solve without it; fewer labels are extended. With no upper bound, only
        /// labels that reach no end node are dropped; a negative-cost cycle leaves no finite
        /// bound, and nothing is dropped.
        ///
        /// Compared with @ref prune_by_incumbent: this prune compares with the solve's upper
        /// bound, which does not change, once per label (the take rule's fixed checks), with a
        /// tolerance margin; the incumbent prune compares with the incumbent, which falls as paths
        /// are found, every time a label is taken. With both on, the incumbent prune drops every
        /// label this one drops, except within the margin.
        ///
        /// The bound needs an additive cost: every component the cost reads has a zero cost or a
        /// value cost under an @c Accumulate extension (@c Resource::is_cost_additive). It also
        /// needs every arc to add at least its share of the cost, whatever the label it extends,
        /// which the solve tests at the start by running the extension on the model's arcs: a
        /// floor, a ceiling or a maximum in a priced extension fails it. A cost that fails either
        /// throws `std::invalid_argument` when the solve starts. Read by every labelling
        /// search, Greedy and the tabu searches included, in every direction, and by both sides of
        /// a bidirectional search, each against its own end nodes.
        bool prune_by_threshold = false;

        /// @brief The per-node quota: at most this many labels are taken at each node in each
        ///        phase (no limit by default, @ref kNoLimit).
        ///
        /// Pushing and Pulling apply it per visit of a node rather than per phase
        /// (@c parts::PerNodeQuota).
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
        /// aside after the last phase make the solve @c AlgorithmStatus::Truncated. A driven
        /// search (the tabu searches) runs up to this many phases in each run; a bidirectional
        /// search runs one.
        size_t max_phases = 1;

        /// @brief The most iterations of one main loop (no limit by default, @ref kNoLimit).
        ///
        /// What an iteration is depends on the algorithm:
        /// - a labelling search (`Simple`, `Pushing`, `AStar`, `Greedy`): one label taken from the
        ///   schedule; `Pulling`: one node visited. The count starts again at each phase, so a
        ///   solve may make up to `max_phases × max_iterations`. A bidirectional search counts
        ///   the iterations of both sides together.
        /// - the tabu searches: one episode (one dive to a path; `ImprovingTabu`'s first dive is
        ///   not counted); the labels a dive takes are not counted. Must be finite.
        /// - `DiversificationSearch`: one solve of the inner algorithm. Must be finite.
        ///
        /// A solve it stops with labels still waiting ends @c AlgorithmStatus::Truncated.
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

        /// @brief How long an arc stays tabu, in episodes of a tabu search (default 5).
        ///
        /// An arc made tabu after an episode stays tabu for the next `tabu_tenure` − 1 episodes,
        /// plus an adaptive extra that each search grows and shrinks by its own rule
        /// (@c parts::TabuEpisodes, @c parts::ImprovingTabuEpisodes). @ref DiversificationSearch
        /// counts its inner solves the same way.
        size_t tabu_tenure = 5;  // NOLINT(readability-magic-numbers)
        /// @brief Nodes whose arcs are never made tabu: an arc that starts or ends at one of them
        ///        stays usable, in the tabu searches and in @ref DiversificationSearch.
        std::set<size_t> tabu_exempt_node_ids;
        /// @brief Whether each tenure is moved at random by −1, 0 or +1 when an arc is made tabu
        ///        (default true). A tenure of 1 is never lowered. The draws come from @ref seed.
        bool tabu_random_noise = true;

        /// @brief How many episodes in a row without a cheaper path make
        ///        @c strategy::ImprovingTabu lengthen its tabu tenure (default 10).
        ///
        /// An episode improves when it finds a path cheaper than the best so far by more than
        /// @c tolerance. @c DiversificationSearch does not read this: it lengthens its tenure
        /// after every inner solve that adds no new path.
        size_t non_improving_episodes_before_growth = 10;  // NOLINT(readability-magic-numbers)

        /// @brief The seed of the random generator behind @ref tabu_random_noise (default 0).
        ///
        /// The generator is seeded once, when the algorithm is built, and is not seeded again
        /// at each solve: two solves with the same algorithm draw different noise.
        int seed = 0;

        /// @brief Index of the critical resource: the "clock" on which the bidirectional search
        ///        sets its half-way point.
        ///
        /// The clock must never decrease along an arc (e.g. time or load). The index counts only
        /// the components of the clock's resource type, which is a template parameter of the
        /// algorithm.
        size_t critical_resource_index = 0;

        /// @brief Half-way point `H` on the critical resource; **0 turns it off**.
        ///
        /// Forward labels stop above `H`, backward labels below it. Set it to about half the
        /// clock's range. With 0, both searches run to their end nodes: correct but slower.
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

        // ── Admission parameters ────────────────────────────────────────────

        /// @brief The rule that decides whether a new label is kept at its node (default:
        ///        `Exact`).
        ///
        /// - `Exact` keeps a label unless a label at its node dominates it.
        /// - `Relaxed` leaves @ref dominance_ignored_components out of dominance: two labels that
        ///   differ only there compare as if they were equal there, so labels are dropped that an
        ///   exact search keeps.
        /// - `KBest` keeps, after exact dominance, at most @ref max_admitted_labels_per_node labels
        ///   at each node, the cheapest. At a full node, a new label takes the place of the
        ///   costliest one if it costs less, and is dropped otherwise. End nodes are nodes too,
        ///   so a solve returns at most k paths for each end node, whatever @ref max_solutions is.
        ///
        /// `Relaxed` and `KBest` are heuristics: they may miss the optimum, and the solve is then
        /// not proven (`ProofLoss::RelaxedAdmission`). Every path returned is still feasible and
        /// correctly priced, since feasibility and the cost never read dominance. A rule at its
        /// limit (no component left out, no limit on k) is exact.
        ///
        /// The exact labelling searches (`Simple`, `Pushing`, `Pulling`, `AStar`) read the rule
        /// in every direction, and so do both sides of a bidirectional search. Greedy and the
        /// tabu searches keep every feasible label (@c parts::AdmitAll) and ignore it. Every
        /// algorithm still throws when the solve starts if the rule cannot be honoured
        /// (@ref max_admitted_labels_per_node 0, or an unknown component in
        /// @ref dominance_ignored_components).
        parts::AdmissionRule admission_rule = parts::AdmissionRule::Exact;

        /// @brief The components (resources of the model) that the `Relaxed` rule leaves out of
        ///        dominance. Other rules ignore it.
        ///
        /// Components are numbered as the model checks' messages number them: by type, in the
        /// order of the `ResourceGraph`'s template parameters, then in registration order within
        /// a type. Dominance is then tested component by component over the other components, as
        /// `CompositionDominanceFunction` does, whatever composition dominance function the model
        /// uses.
        ///
        /// With `LabelBuckets`, leaving out the bucket or sort resource weakens the relaxation,
        /// since the buckets' early exits assume that those resources order dominance. Leaving
        /// out the bidirectional clock turns the half-way point off, since a dominating label may
        /// then lie past `H`. Under `Relaxed`, naming a component the model does not have throws
        /// `std::invalid_argument` when the solve starts.
        std::set<size_t> dominance_ignored_components;

        /// @brief The most labels each node keeps under the `KBest` rule (default: no limit, so
        ///        every label dominance keeps is kept).
        ///
        /// It must be at least 1: with 0, a `KBest` solve throws `std::invalid_argument` when it
        /// starts. It applies at the end nodes too, so a solve returns at most this many paths
        /// for each end node.
        size_t max_admitted_labels_per_node = kNoLimit;

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

        /// @brief How often the search checks how much memory the process uses: once every
        ///        this many main-loop iterations (default 50,000).
        ///
        /// The check reads the process's resident set size (RSS: the physical memory it
        /// occupies) and compares it with the memory limit and the pressure threshold
        /// (@ref memory_limit_fraction, @ref memory_pressure_fraction). Larger values reduce the
        /// cost of measuring; smaller values react faster to memory spikes. Must be > 0.
        ///
        /// Iterations are counted as for @ref max_iterations, and the count starts again at each
        /// phase, so a phase shorter than this never checks the memory. A driven search (the tabu
        /// searches) counts the iterations of all its runs in the solve together.
        /// @ref DiversificationSearch ignores this interval: it checks the memory once after each
        /// inner solve.
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
        /// (@ref memory_pressure_fraction), the search trims the labels waiting to be extended:
        /// - `Pushing` and `Pulling` keep this many at each node;
        /// - `Simple`, `AStar` and `Greedy` keep this many times the number of nodes in all. They
        ///   keep the best: non-dominated first, then cheapest (A* by its key). Greedy's dive
        ///   keeps the labels it would take first.
        ///
        /// Of the labels trimmed, a dominated one is released. A non-dominated one is stored aside
        /// for the next phase, or abandoned on a side of a bidirectional search. From the second
        /// trim of a solve on, every label stored aside is released first, to free memory. The
        /// per-node quota (@ref max_extended_labels_per_node) is also lowered to this value for
        /// the rest of the solve. The result is then not a proof (@c ProofLoss::MemoryPressure).
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

    private:
        /// @brief Warns about an admission field the chosen rule does not read, and about a rule
        ///        set at its limit, where it is exact.
        void check_admission() const {
            if (admission_rule != parts::AdmissionRule::Relaxed &&
                !dominance_ignored_components.empty()) {
                LOG_WARN(
                    "AlgorithmParams: dominance_ignored_components is set but admission_rule is "
                    "not Relaxed. dominance_ignored_components will not have any effects.\n");
            }
            if (admission_rule != parts::AdmissionRule::KBest &&
                max_admitted_labels_per_node < kNoLimit) {
                LOG_WARN(
                    "AlgorithmParams: max_admitted_labels_per_node is set but admission_rule is "
                    "not KBest. "
                    "max_admitted_labels_per_node will not have any effects.\n");
            }
            if (admission_rule == parts::AdmissionRule::Relaxed &&
                dominance_ignored_components.empty()) {
                LOG_WARN(
                    "AlgorithmParams: admission_rule is Relaxed but dominance_ignored_components "
                    "is empty, so dominance is exact.\n");
            }
            if (admission_rule == parts::AdmissionRule::KBest &&
                max_admitted_labels_per_node >= kNoLimit) {
                LOG_WARN(
                    "AlgorithmParams: admission_rule is KBest but max_admitted_labels_per_node == "
                    "MAX, so every "
                    "label dominance keeps is kept.\n");
            }
        }
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
