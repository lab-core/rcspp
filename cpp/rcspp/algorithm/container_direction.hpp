// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

#include "rcspp/algorithm/direction.hpp"

namespace rcspp::detail {

/// @brief Whether label container @p LabelContainerType declares its own
///        `with_direction<Direction>()`: an empty copy of itself, configured the same, that
///        compares labels in direction @p Direction.
///
/// The member must be declared by @p LabelContainerType, not inherited: a container derived from
/// @c LabelList that does not declare it would otherwise be swapped for a plain list. A member
/// function inherited from a base has the base's member-pointer type, which the second test
/// rejects.
///
/// @tparam LabelContainerType The container type.
/// @tparam Direction          The direction policy.
template <typename LabelContainerType, typename Direction>
concept RebindsDirection =
    requires(const LabelContainerType& container) {
        container.template with_direction<Direction>();
    } && std::same_as<decltype(&LabelContainerType::template with_direction<Direction>),
                      decltype(std::declval<const LabelContainerType&>()
                                   .template with_direction<Direction>()) (LabelContainerType::*)()
                          const>;

/// @brief The container a search in @p Direction keeps, given the container type
/// @p LabelContainerType its params hold.
///
/// A container checks dominance itself, and a backward label dominates in the reverse order on a
/// threshold resource, so a backward search cannot keep the caller's forward container: it needs
/// the same container comparing backward. Forward keeps @p LabelContainerType, whatever it is. Any
/// other direction asks the container for a copy of itself in that direction
/// (@ref RebindsDirection); a container that cannot make one has no rebinding, and a search in that
/// direction cannot be built with it.
///
/// @tparam LabelContainerType The container type of the algorithm's params.
/// @tparam Direction          The search's direction policy.
template <typename LabelContainerType, typename Direction>
struct rebind_direction;

template <typename LabelContainerType>
struct rebind_direction<LabelContainerType, ForwardDirection> {
        using type = LabelContainerType;

        /// @brief An empty container configured as @p prototype.
        static LabelContainerType convert(const LabelContainerType& prototype) {
            return prototype.copy();
        }
};

// Hidden from Doxygen: Sphinx's C++ domain cannot parse a partial specialization with a requires
// clause, and crashes on the page Exhale makes for it, which fails the GitHub Pages build.
/// @cond
template <typename LabelContainerType, typename Direction>
    requires(!std::is_same_v<Direction, ForwardDirection> &&
             RebindsDirection<LabelContainerType, Direction>)
struct rebind_direction<LabelContainerType, Direction> {
        using type = decltype(std::declval<const LabelContainerType&>()
                                  .template with_direction<Direction>());

        /// @brief An empty container configured as @p prototype, comparing in @p Direction.
        static type convert(const LabelContainerType& prototype) {
            return prototype.template with_direction<Direction>();
        }
};
/// @endcond

template <typename LabelContainerType, typename Direction>
using rebind_direction_t = typename rebind_direction<LabelContainerType, Direction>::type;

/// @brief Whether a search in @p Direction can keep its labels in @p LabelContainerType's
/// rebinding.
template <typename LabelContainerType, typename Direction>
inline constexpr bool can_rebind_direction_v = requires {
    typename rebind_direction<LabelContainerType, Direction>::type;
};  // NOLINT(whitespace/newline)

}  // namespace rcspp::detail
