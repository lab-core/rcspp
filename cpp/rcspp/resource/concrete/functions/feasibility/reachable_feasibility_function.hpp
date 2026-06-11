// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>
#include <memory>
#include <set>
#include <type_traits>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

/// @brief Feasibility function that checks reachability of required destination nodes.
///
/// Labels are always locally feasible (`is_feasible` returns `true`).  The
/// reachability check verifies that, for every destination node that must be
/// visited (i.e., it appears in `checked_nodes`), the current resource already
/// contains that node.  Destination nodes not listed in `checked_nodes` are
/// unconditionally reachable.
///
/// @tparam ContainerResourceType The resource type whose value is a container
///         supporting `contains(size_t node_id)`.
// Deduce the container/value type by calling get_value() on the concrete Resource
template <typename ContainerResourceType>
class ReachableFeasibilityFunction
    : public Clonable<ReachableFeasibilityFunction<ContainerResourceType>,
                      FeasibilityFunction<ContainerResourceType>> {
    public:
        /// @brief Constructs the function with the set of nodes whose reachability must be
        ///        verified.
        ///
        /// @param checked_nodes A container resource instance whose value represents the set
        ///        of node ids that the label must have visited before reaching each
        ///        destination.
        explicit ReachableFeasibilityFunction(ContainerResourceType checked_nodes)
            : checked_nodes_(
                  std::make_shared<const ContainerResourceType>(std::move(checked_nodes))) {}

        /// @brief Always returns `true`; reachability is enforced via `is_reachable`.
        ///
        /// @param resource Unused resource parameter.
        /// @return `true` unconditionally.
        auto is_feasible(const ContainerResourceType& /*resource*/) -> bool override {
            return true;
        }

        /// @brief Checks that a required destination node is reachable from the current label.
        ///
        /// A destination is reachable if either it is not in `checked_nodes` (not required) or
        /// if the label's resource already contains it.
        ///
        /// @param resource The current label's resource.
        /// @param destination_node_id The id of the node being tested for reachability.
        /// @return `true` if the destination is reachable (or not required to have been visited).
        auto is_reachable(const Resource<ContainerResourceType>& resource,
                          size_t destination_node_id) -> bool override {
            // either not to be checked (i.e., not required) or contained in the reachable set
            return !checked_nodes_->contains(destination_node_id) ||
                   resource.contains(destination_node_id);
        }

    private:
        std::shared_ptr<const ContainerResourceType> checked_nodes_;
};
}  // namespace rcspp
