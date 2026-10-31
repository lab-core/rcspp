// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

#include "rcspp/algorithm/direction.hpp"

namespace rcspp::detail {

/// @brief Whether a search in @p Direction can keep its labels in a container of type
///        @p LabelContainerType.
///
/// A container checks dominance itself, and a backward label dominates in the reverse order on a
/// threshold resource, so a backward search cannot keep the caller's forward container: it needs
/// the same container comparing backward. Forward keeps any container. Any other direction needs
/// the container to declare its own `with_direction<Direction>()`: an empty copy of itself,
/// configured the same, that compares labels in @p Direction.
///
/// The member must be declared by @p LabelContainerType, not inherited: a container derived from
/// @c LabelList that does not declare it would otherwise be swapped for a plain list. A member
/// function inherited from a base has the base's member-pointer type, which the second test
/// rejects.
///
/// @tparam LabelContainerType The container type of the algorithm's params.
/// @tparam Direction          The search's direction policy.
template <typename LabelContainerType, typename Direction>
inline constexpr bool has_directed_container_v =
    std::is_same_v<Direction, ForwardDirection> ||
    (requires(const LabelContainerType& container) {
        container.template with_direction<Direction>();
    } && std::same_as<decltype(&LabelContainerType::template with_direction<Direction>),
                      decltype(std::declval<const LabelContainerType&>()
                                   .template with_direction<Direction>()) (LabelContainerType::*)()
                          const>);

/// @brief An empty container configured as @p prototype, comparing labels in @p Direction: a copy
///        of @p prototype forward, its `with_direction<Direction>()` otherwise.
///
/// @tparam Direction          The search's direction policy.
/// @tparam LabelContainerType The container type of the algorithm's params; a search in
///                            @p Direction must be able to keep it (@ref has_directed_container_v).
template <typename Direction, typename LabelContainerType>
auto make_directed_container(const LabelContainerType& prototype) {
    static_assert(has_directed_container_v<LabelContainerType, Direction>,
                  "a search in this direction needs a container that declares with_direction");
    if constexpr (std::is_same_v<Direction, ForwardDirection>) {
        return static_cast<LabelContainerType>(prototype.copy());
    } else {
        return prototype.template with_direction<Direction>();
    }
}

/// @brief The container a search in @p Direction keeps, given the container type
///        @p LabelContainerType its params hold (@ref make_directed_container).
template <typename LabelContainerType, typename Direction>
using directed_container_t =
    decltype(make_directed_container<Direction>(std::declval<const LabelContainerType&>()));

}  // namespace rcspp::detail
