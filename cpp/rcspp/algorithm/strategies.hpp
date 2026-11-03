// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/diversification_search.hpp"
#include "rcspp/algorithm/improving_tabu_search.hpp"
#include "rcspp/algorithm/labelling/parts/admission/admit_all.hpp"
#include "rcspp/algorithm/labelling/parts/retention/keep_while_waiting.hpp"
#include "rcspp/algorithm/labelling/parts/retention/retention.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/best_first_schedule.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/depth_first_schedule.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/fifo_schedule.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/pull_schedule.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/sweep_schedule.hpp"
#include "rcspp/algorithm/tabu_search.hpp"

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
/// - `static constexpr bool backward = false;` or `bidirectional = false;` the strategy refuses
///   that direction: @c create_algorithm then throws @c std::invalid_argument when asked for it.
///
/// The first four are templates on the resource type and the search's direction, so a strategy
/// can use a different part forward and backward; a plain type there is refused at compile time.
/// @c interleave is a plain type. The direction itself is chosen at run time, with
/// @c params.direction. The library's strategies below are written exactly as one's own would
/// be; docs/advanced/algorithms.md, "A strategy of one's own", gives examples and the rules the
/// concepts cannot check.
///
/// Greedy is a dive, built from the depth-first parts; it searches forward only.
///
/// The other heuristics (the tabu searches, diversification) are not labelling searches: their
/// descriptors name a class of their own,
/// `template <typename ResourceType, typename LabelContainerType> using algorithm = ...;`, and
/// search forward only.
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

/// @brief A greedy dive: depth first, the cheapest child first, every feasible label kept and
///        none behind; at a dead end it goes back to the last label still waiting, and each path
///        to a sink is recorded when its label is taken. Forward only.
///
/// @c max_iterations counts the labels taken, as in every labelling search.
struct Greedy {
        template <typename ResourceType, typename Direction>
        using schedule = parts::DepthFirstSchedule<ResourceType>;
        template <typename ResourceType, typename Direction>
        using admission = parts::AdmitAll;
        template <typename ResourceType, typename Direction>
        using retention = parts::Retain<parts::KeepWhileWaiting>;
        static constexpr bool backward = false;
        static constexpr bool bidirectional = false;
};

/// @brief Dives that make the arcs they use tabu (@ref TabuSearchAlgorithm), forward only.
struct Tabu {
        template <typename ResourceType, typename LabelContainerType>
        using algorithm = TabuSearchAlgorithm<ResourceType, LabelContainerType>;
};

/// @brief Tabu dives that improve on the incumbent (@ref ImprovingTabuSearch), forward only.
struct ImprovingTabu {
        template <typename ResourceType, typename LabelContainerType>
        using algorithm = ImprovingTabuSearch<ResourceType, LabelContainerType>;
};

/// @brief Repeated solves of @p Inner, forward only, each solve leaving out (as hard arcs of the
///        inner algorithm's mask) the arcs of the paths found in the last @c tabu_tenure solves
///        (@ref DiversificationSearch).
///
/// @c max_iterations counts the inner solves, and must be finite. Each inner solve keeps one
/// path and takes at most 1000 labels, whatever the params say.
///
/// @tparam Inner The strategy solved each time; Greedy by default.
template <typename Inner = Greedy>
struct Diversification {
        template <typename ResourceType, typename LabelContainerType>
        using algorithm = DiversificationSearch<ResourceType, LabelContainerType, Inner>;
};

}  // namespace rcspp::strategy
