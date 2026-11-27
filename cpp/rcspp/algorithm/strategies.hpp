// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/diversification_search.hpp"
#include "rcspp/algorithm/labelling/parts/admission/admit_all.hpp"
#include "rcspp/algorithm/labelling/parts/driver/improving_tabu_episodes.hpp"
#include "rcspp/algorithm/labelling/parts/driver/tabu_episodes.hpp"
#include "rcspp/algorithm/labelling/parts/retention/keep_while_waiting.hpp"
#include "rcspp/algorithm/labelling/parts/retention/retention.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/best_first_schedule.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/cost_key.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/cost_plus_bound_key.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/depth_first_schedule.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/fifo_schedule.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/pull_schedule.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/sweep_schedule.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/tabu_child_filter.hpp"

/// @brief The strategies a solve names: @c graph.solve<strategy::AStar>(...).
///
/// A labelling strategy is a descriptor: a struct whose members name the parts its search is
/// built from. @c create_algorithm reads these members, and no others:
///
/// - `template <typename ResourceType, typename Direction> using schedule = ...;` the order in
///   which labels are taken (required);
/// - `template <typename ResourceType, typename Direction> using admission = ...;` whether a new
///   label is kept at its node (default: @c parts::DominanceAdmission);
/// - `template <typename ResourceType, typename Direction> using retention = ...;` where kept
///   labels live, as a type whose member template `type<ContainerType>` is the retention
///   (default: @c parts::KeepAtNodes);
/// - `template <typename ResourceType, typename Direction> using observer = ...;` told of every
///   extension (default: @c parts::NoObserver);
/// - `using interleave = ...;` which side of a bidirectional search works next (default:
///   @c parts::LeastWorkFirst);
/// - `using driver = ...;` runs the search several times in one solve, each run up to
///   @c max_phases phases (@c parts::Driver; default: none, one run). A strategy with a driver
///   never searches bidirectionally;
/// - `static constexpr bool backward = false;` or `bidirectional = false;` the strategy refuses
///   that direction: @c create_algorithm then throws @c std::invalid_argument when asked for it.
///
/// The first four are templates on the resource type and the search's direction, so a strategy
/// can use a different part forward and backward; a plain type there is refused at compile time.
/// @c interleave and @c driver are plain types. The direction itself is chosen at run time, with
/// @c params.direction. The library's strategies below are written exactly as one's own would
/// be; docs/advanced/algorithms.md, "A strategy of one's own", gives examples and the rules the
/// concepts cannot check.
///
/// Greedy, Tabu and ImprovingTabu are dives, built from the depth-first parts; the tabu searches
/// add a driver. They search forward or backward, never bidirectionally.
///
/// Diversification is not a labelling search: its descriptor names a class of its own,
/// `template <typename ResourceType, typename LabelContainerType> using algorithm = ...;`, and
/// searches forward only.
namespace rcspp::strategy {

/// @brief Labels taken first in, first out.
struct Simple {
        template <typename ResourceType, typename Direction>
        using schedule = parts::FifoSchedule<ResourceType>;
};

/// @brief Labels taken by cost plus completion bound, lowest first (A*).
///
/// A label's completion bound is a lower bound on the cost still to pay from its node to an end
/// node: the shortest distance with the resource constraints dropped, computed at the start of
/// each solve by one Bellman–Ford run from the end nodes, each arc weighted by the cost a label
/// gains along it (@c detail::compute_completion_bound, in labelling/completion_bound.hpp).
struct AStar {
        template <typename ResourceType, typename Direction>
        using schedule = parts::BestFirstSchedule<ResourceType>;
};

/// @brief Labels taken node by node, in the graph's node order (@c Node::pos(), as
///        @c sort_nodes_by_connectivity sets it, or by id): ascending forward, descending
///        backward, sweep after sweep until no label waits.
struct Pushing {
        template <typename ResourceType, typename Direction>
        using schedule = parts::SweepSchedule<ResourceType, Direction>;
};

/// @brief One node visited at a time, in Pushing's node order, pulling into it the labels
///        waiting at its neighbours: its predecessors forward, its successors backward.
struct Pulling {
        template <typename ResourceType, typename Direction>
        using schedule = parts::PullSchedule<ResourceType, Direction>;
};

/// @brief A greedy dive: a depth-first search that takes the cheapest child first. It searches
///        forward from the sources or backward from the sinks, not bidirectionally.
///
/// Every feasible label is kept, with no dominance test (@c parts::AdmitAll), but only while it
/// waits to be taken: no label stays at the nodes behind the dive (@c parts::KeepWhileWaiting).
/// At a dead end, the dive goes back to the last label still waiting. A path is recorded when
/// its label at an end node is taken.
///
/// @c max_iterations counts the labels taken. With no limit (the default), Greedy tries every
/// path, so it finds the optimum, but its work grows with the number of paths.
struct Greedy {
        template <typename ResourceType, typename Direction>
        using schedule = parts::DepthFirstSchedule<ResourceType>;
        template <typename ResourceType, typename Direction>
        using admission = parts::AdmitAll;
        template <typename ResourceType, typename Direction>
        using retention = parts::Retain<parts::KeepWhileWaiting>;
        static constexpr bool bidirectional = false;
};

/// @brief Greedy's dive, but each label's children are taken by cost plus completion bound
///        (g + h, A*'s key; @c parts::CostPlusBoundKey), lowest first, so that the dive heads
///        toward the cheapest completion. Forward or backward, not bidirectionally.
///
/// Like Greedy, it is a heuristic: it has no dominance test, and its result is a proof only if
/// the dive runs out of paths. It is meant to be used with @c prune_by_threshold, which uses the
/// same bound to drop the labels that cannot beat the upper bound.
///
/// @c max_iterations counts the labels taken, as in Greedy. The completion bound is A*'s (see
/// AStar), computed at the start of each solve.
struct BoundGuidedGreedy {
        template <typename ResourceType, typename Direction>
        using schedule = parts::DepthFirstSchedule<ResourceType, parts::CostPlusBoundKey>;
        template <typename ResourceType, typename Direction>
        using admission = parts::AdmitAll;
        template <typename ResourceType, typename Direction>
        using retention = parts::Retain<parts::KeepWhileWaiting>;
        static constexpr bool bidirectional = false;
};

/// @brief A tabu search: Greedy's dive, run in episodes. Each episode stops at the first path it
///        finds, and the arcs of that path are then tabu for the next episodes, so that the
///        next dives look elsewhere. Forward or backward, never bidirectionally.
///
/// An arc stays tabu for @c tabu_tenure − 1 episodes, plus an adaptive extra that grows after
/// an episode that found nothing new and shrinks after one that found a new path
/// (@c parts::TabuEpisodes). The dive drops a child reached along a tabu arc, unless every child
/// is (@c parts::TabuChildFilter).
///
/// @c max_iterations counts episodes, and must be finite. With no limit, the solve logs an error
/// and runs no episode. With 0, it runs no episode either, and returns nothing.
struct Tabu {
        template <typename ResourceType, typename Direction>
        using schedule =
            parts::DepthFirstSchedule<ResourceType, parts::CostKey, parts::TabuChildFilter>;
        template <typename ResourceType, typename Direction>
        using admission = parts::AdmitAll;
        template <typename ResourceType, typename Direction>
        using retention = parts::Retain<parts::KeepWhileWaiting>;
        using driver = parts::TabuEpisodes;
};

/// @brief Tabu's episodes after a first dive with no arc tabu (the construction), with a tenure
///        that adapts to whether the episodes improve on the best path
///        (@c parts::ImprovingTabuEpisodes). Forward or backward.
///
/// An episode improves when it finds a path strictly cheaper than the best so far; the tabu
/// tenure then shortens. After @c non_improving_episodes_before_growth episodes in a row that
/// do not improve, the tenure grows. Each episode still stops at its first path, whatever it
/// costs.
///
/// @c max_iterations counts the episodes after the construction, and must be finite: with no
/// limit, the solve logs an error and runs nothing. With 0, only the construction runs, and its
/// path is returned.
struct ImprovingTabu {
        template <typename ResourceType, typename Direction>
        using schedule =
            parts::DepthFirstSchedule<ResourceType, parts::CostKey, parts::TabuChildFilter>;
        template <typename ResourceType, typename Direction>
        using admission = parts::AdmitAll;
        template <typename ResourceType, typename Direction>
        using retention = parts::Retain<parts::KeepWhileWaiting>;
        using driver = parts::ImprovingTabuEpisodes;
};

/// @brief Solves @p Inner again and again, forward only (@ref DiversificationSearch). The arcs
///        of each path found are left out of the next @c tabu_tenure − 1 solves, plus an
///        adaptive extra, so that each solve finds a different path.
///
/// The arcs left out are hard arcs of the inner algorithm's arc mask; arcs with an end in
/// @c tabu_exempt_node_ids are never left out. @c max_iterations counts the inner solves, and
/// must be finite. Each inner solve keeps one path, whatever the params say, and has its own
/// budget (@ref DiversificationSearch::inner_iteration_budget): 20 labels taken per node of the
/// graph and never fewer than 1000 for a labelling search such as Greedy; 1000 episodes for a
/// driven search such as Tabu.
///
/// @tparam Inner The strategy solved each time; Greedy by default.
template <typename Inner = Greedy>
struct Diversification {
        template <typename ResourceType, typename LabelContainerType>
        using algorithm = DiversificationSearch<ResourceType, LabelContainerType, Inner>;
};

}  // namespace rcspp::strategy
