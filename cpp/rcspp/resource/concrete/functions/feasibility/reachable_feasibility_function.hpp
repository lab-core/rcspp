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

        /// @brief @c Unspecified: this class has no backward semantics, so a bidirectional solve
        ///        refuses to start on it.
        ///
        /// The merge itself is unconstrained; the problem is @ref is_reachable, a predicate on the
        /// prefix that would be misapplied to a backward label's suffix. @c merge_rule() is where
        /// the solver looks for that refusal. Forward-only use is unaffected.
        ///
        /// @return @c MergeRule::Unspecified.
        [[nodiscard]] MergeRule merge_rule() const override { return MergeRule::Unspecified; }

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
            const auto node =
                static_cast<typename ContainerResourceType::ValueType>(destination_node_id);
            return !checked_nodes_->contains(node) || resource.get_value().contains(node);
        }

    private:
        std::shared_ptr<const ContainerResourceType> checked_nodes_;
};

/// A container feasibility function has no scalar bound, so it never seeds a backward label.
template <typename R>
struct BackSeedEndOf<ReachableFeasibilityFunction<R>> {
        static constexpr BackSeedEnd value = BackSeedEnd::Never;
};

}  // namespace rcspp
