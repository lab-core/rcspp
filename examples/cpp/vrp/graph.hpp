// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The pricing graph of a vehicle routing problem with time windows.

#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

#include "rcspp/rcspp.hpp"
#include "vrp/instance.hpp"

namespace vrp {

/// @brief The graph's resource types: real values (distance, time, load) and, for a route
///        memory, a set of nodes.
using Graph = rcspp::ResourceGraph<rcspp::RealResource, rcspp::SizeTBitsetResource>;

/// @brief Which routes pricing searches.
enum class Relaxation {
    None,        ///< resource-feasible routes; a route may visit a customer twice
    NgPath,      ///< ng-routes: no return to a customer while it is in the route's memory
    Elementary,  ///< no customer visited twice; much slower
};

/// @brief The relaxation @p name gives (`none`, `ng`, `elementary`), or nothing.
[[nodiscard]] std::optional<Relaxation> parse_relaxation(std::string_view name);

/// @brief The ng neighbourhood's size when none is given.
inline constexpr size_t default_ng_size = 8;

/// @brief The sink's node: the depot again, as the end of a route, numbered after the last
///        customer.
[[nodiscard]] size_t sink(const Instance& instance);

/// @brief A route is a path from the depot (node 0) to the sink, through customers 1 to n.
///
/// Resources: the distance, which is the cost; the time, within each customer's window (travel
/// plus the service at the customer left); the load, within the capacity; with @c NgPath or
/// @c Elementary, a route memory. Without one, a route may visit a customer twice: column
/// generation over these routes gives a weaker bound than over elementary ones, and a faster
/// search. Ng-routes are in between: each customer's neighbourhood is its @p ng_size nearest
/// customers, itself included.
///
/// The master has one row per customer, row `i - 1` for customer `i`: an arc out of a customer
/// covers its row.
///
/// The graph is filled in place: a graph holds its resources, so it is not copied.
void pricing_graph(const Instance& instance, Graph& graph, Relaxation relaxation = Relaxation::None,
                   size_t ng_size = default_ng_size);

}  // namespace vrp
