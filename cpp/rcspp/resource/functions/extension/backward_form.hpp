// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

/// @brief A backward form: how an extension function is read backward, packaged so that a
///        concrete function supplies only its data.
///
/// A bidirectional solve also runs the search backward, from the sinks, so every extension
/// function needs a backward reading, its @c BackwardKind. A form fixes that reading once. It is
/// a mixin placed between @c ExtensionFunction<R> and the concrete function, as the middle
/// argument of a three-argument @c Clonable:
///
/// @code
/// class MyExtension
///     : public Clonable<MyExtension, SomeForm<..., ExtensionFunction<R>>, ExtensionFunction<R>> {
///     ...
/// };
/// @endcode
///
/// Use this class directly when the inherited @c extend_back is already right:
/// `BackwardForm<ExtensionFunction<R>, BackwardKind::Accumulate>` for an addition,
/// `BackwardForm<ExtensionFunction<R>, BackwardKind::ArcValue>` for a set filled from arc values.
/// Derive a form from it when the kind needs steps of its own: @c ThresholdForm writes
/// @c extend and @c extend_back and asks for per-node bounds; @c ArcEndpointsForm writes them
/// and asks for one formula per side of the arc.
///
/// To write a new form, derive from `BackwardForm<Base, Kind>`, write the steps the kind needs
/// as @c final (with @c start_back if a backward label must not start at the type default), and
/// leave the resource-specific pieces pure virtual. A form for an existing kind is
/// self-contained; a new @c BackwardKind also needs @ref reverses_back_dominance to say how its
/// backward labels compare, and the feasibility functions to say how they join.
///
/// @warning @c BackwardKind::ArcValue asserts the arc's value is genuine per-arc data. A node
///          identity such as `{origin}` offsets the backward memory by one node; use
///          @c ArcEndpointsForm for that shape.
///
/// @tparam Base The base to insert above, normally @c ExtensionFunction<R>.
/// @tparam Kind The kind this form declares. @c Unspecified only from a form whose kind depends on
///              its arguments, such as @c ThresholdForm over an unsigned type.
template <typename Base, BackwardKind Kind>
class BackwardForm : public Base {
    public:
        /// @brief This form's backward kind, readable at compile time (@c backward_kind_of_v).
        static constexpr BackwardKind kind = Kind;

        /// @return @ref kind.
        [[nodiscard]] BackwardKind backward_kind() const final { return kind; }
};

}  // namespace rcspp
