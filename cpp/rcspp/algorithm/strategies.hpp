// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/diversification_search.hpp"
#include "rcspp/algorithm/labelling/node_schedules.hpp"
#include "rcspp/algorithm/labelling/policies.hpp"
#include "rcspp/algorithm/labelling/schedules.hpp"
#include "rcspp/algorithm/labelling/tabu_episodes.hpp"

/// @brief The strategies a solve names: @c graph.solve<strategy::AStar>(...).
///
/// A labelling strategy is a descriptor: a struct that names its schedule, given the resource
/// type and the search's direction, and may name its admission rule, its retention, its observer,
/// a bidirectional search's interleave, and a driver that runs it several times in one solve
/// (@c parts::Driver); each part it does not name is the exact search's (@c rcspp::parts), and
/// with no driver it runs once, then its phases. A strategy that cannot search in a direction says
/// so with
/// `static constexpr bool backward = false` or `bidirectional = false`. The direction itself is
/// chosen at run time, with @c params.direction. A strategy of one's own is written the same way.
///
/// Greedy and the tabu searches are descriptors too: dives, from the depth-first parts, the tabu
/// searches run by their drivers. They search forward or backward, never bidirectionally.
/// Diversification keeps a class of its own, which its descriptor names as `algorithm`; it
/// searches forward only.
namespace rcspp::strategy {

/// @brief Labels taken first in, first out.
struct Simple {
        template <typename ResourceType, typename Direction>
        using schedule = parts::FifoSchedule<ResourceType>;
};

/// @brief Labels taken by cost plus completion bound, lowest first (A*).
struct AStar {
        template <typename ResourceType, typename Direction>
        using schedule = parts::BestFirstSchedule<ResourceType>;
};

/// @brief Labels taken node by node, in the direction's sweep order: ascending positions forward,
///        descending backward.
struct Pushing {
        template <typename ResourceType, typename Direction>
        using schedule = parts::SweepSchedule<ResourceType, Direction>;
};

/// @brief One node visited at a time, in the direction's sweep order, pulling from its
///        neighbours: its predecessors forward, its successors backward.
struct Pulling {
        template <typename ResourceType, typename Direction>
        using schedule = parts::PullSchedule<ResourceType, Direction>;
};

/// @brief A greedy dive: depth first, the cheapest child first, every feasible label kept and
///        none behind; at a dead end it goes back to the last label still waiting, and each path
///        to an end node is recorded when its label is taken. Forward, from the sources, or
///        backward, from the sinks; not bidirectionally.
///
/// @c max_iterations counts the labels taken, as in every labelling search.
struct Greedy {
        template <typename ResourceType, typename Direction>
        using schedule = parts::DfsSchedule<ResourceType>;
        template <typename ResourceType, typename Direction>
        using admission = parts::KeepAll;
        template <typename ResourceType, typename Direction>
        using retention = parts::Retain<parts::KeepWhileWaiting>;
        static constexpr bool bidirectional = false;
};

/// @brief Greedy's dive with the children taken by cost plus completion bound, lowest first
///        (@c parts::CompletionKey), not by cost alone: each step goes toward the cheapest
///        completion. Meant with @c threshold_pruning, which drops, under the same bound, the
///        labels that cannot beat the upper bound. Forward or backward; not bidirectionally.
///
/// @c max_iterations counts the labels taken, as in Greedy. The bound is computed at the start of
/// each solve: one Bellman–Ford run over the arcs' shares of the cost, from the end nodes.
struct BoundedGreedy {
        template <typename ResourceType, typename Direction>
        using schedule = parts::DfsSchedule<ResourceType, parts::CompletionKey>;
        template <typename ResourceType, typename Direction>
        using admission = parts::KeepAll;
        template <typename ResourceType, typename Direction>
        using retention = parts::Retain<parts::KeepWhileWaiting>;
        static constexpr bool bidirectional = false;
};

/// @brief Episodes of Greedy's dive, each ending at its first path, whose arcs are then tabu for
///        @c tabu_tenure episodes: a child reached along a tabu arc is dropped unless every child
///        is (@c parts::TabuEpisodes, @c parts::TabuChildFilter). Forward or backward; a driven
///        strategy never searches bidirectionally.
///
/// @c max_iterations counts episodes, and must be finite.
struct Tabu {
        template <typename ResourceType, typename Direction>
        using schedule = parts::DfsSchedule<ResourceType, parts::CostKey, parts::TabuChildFilter>;
        template <typename ResourceType, typename Direction>
        using admission = parts::KeepAll;
        template <typename ResourceType, typename Direction>
        using retention = parts::Retain<parts::KeepWhileWaiting>;
        using driver = parts::TabuEpisodes;
};

/// @brief Tabu's episodes after a first dive with no arc tabu, counting the episodes that do not
///        improve on the best cost (@c parts::ImproveEpisodes). Forward or backward.
///
/// @c max_iterations counts the episodes after the first dive, and must be finite.
struct ImprovingTabu {
        template <typename ResourceType, typename Direction>
        using schedule = parts::DfsSchedule<ResourceType, parts::CostKey, parts::TabuChildFilter>;
        template <typename ResourceType, typename Direction>
        using admission = parts::KeepAll;
        template <typename ResourceType, typename Direction>
        using retention = parts::Retain<parts::KeepWhileWaiting>;
        using driver = parts::ImproveEpisodes;
};

/// @brief Repeated solves of @p Inner, each with the arcs of the paths found so far masked
///        (@ref DiversificationSearch), forward only.
///
/// @tparam Inner The strategy solved each time; Greedy by default.
template <typename Inner = Greedy>
struct Diversification {
        template <typename ResourceType, typename LabelContainerType>
        using algorithm = DiversificationSearch<ResourceType, LabelContainerType, Inner>;
};

}  // namespace rcspp::strategy
