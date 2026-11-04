// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/diversification_search.hpp"
#include "rcspp/algorithm/improving_tabu_search.hpp"
#include "rcspp/algorithm/labelling/node_schedules.hpp"
#include "rcspp/algorithm/labelling/policies.hpp"
#include "rcspp/algorithm/labelling/schedules.hpp"
#include "rcspp/algorithm/tabu_search.hpp"

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
/// Greedy is a descriptor too: a dive, from the depth-first parts. The other heuristics (the tabu
/// searches, diversification) keep classes of their own, which their descriptors name as
/// `algorithm`. Every heuristic searches forward only.
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
///        to a sink is recorded when its label is taken. Forward only.
///
/// @c max_iterations counts the labels taken, as in every labelling search.
struct Greedy {
        template <typename ResourceType, typename Direction>
        using schedule = parts::DfsSchedule<ResourceType>;
        template <typename ResourceType, typename Direction>
        using admission = parts::KeepAll;
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
