// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

/// @brief A directional extension form: how an extension function is read in both directions,
///        packaged so that a concrete function supplies only its data.
///
/// A backward or bidirectional solve runs the search backward, from the sinks, so every extension
/// function needs a backward reading, its @c DirectionalResourceKind. A form fixes that reading
/// once. It is a mixin placed between @c ExtensionFunction<ResourceType> and the concrete function,
/// as the middle argument of a three-argument @c Clonable:
///
/// @code
/// class MyExtension : public Clonable<MyExtension,
///                                     SomeForm<..., ExtensionFunction<ResourceType>>,
///                                     ExtensionFunction<ResourceType>> {
///     ...
/// };
/// @endcode
///
/// Use this class directly when the inherited @c extend_back is already right:
/// `DirectionalExtensionForm<ExtensionFunction<ResourceType>, DirectionalResourceKind::Accumulate>`
/// for an addition,
/// `DirectionalExtensionForm<ExtensionFunction<ResourceType>, DirectionalResourceKind::ArcValue>`
/// for a set filled from arc values. Derive a form from it when the kind needs steps of its own:
/// @c ThresholdExtensionForm writes @c extend and @c extend_back and asks for per-node bounds;
/// @c ArcEndpointsExtensionForm writes them and asks for one formula per side of the arc.
///
/// To write a new form, derive from `DirectionalExtensionForm<Base, Kind>`, write the steps the
/// kind needs as @c final (with @c start_back if a backward label must not start at the type
/// default), and leave the resource-specific pieces pure virtual. A form for an existing kind is
/// self-contained; a new @c DirectionalResourceKind also needs @ref reverses_back_dominance to say
/// how its backward labels compare, and the feasibility functions to say how they join.
///
/// @warning @c DirectionalResourceKind::ArcValue assumes the arc's value is real per-arc data. An
///          arc value that names one of its nodes, such as `{origin}`, makes a backward label add
///          the node it arrives at instead of the node it leaves; use @c ArcEndpointsExtensionForm
///          for that case.
///
/// @tparam Base The base to insert above, normally @c ExtensionFunction<ResourceType>.
/// @tparam Kind The kind this form declares. @c Unspecified only from a form whose kind depends on
///              its arguments, such as @c ThresholdExtensionForm over an unsigned type.
template <typename Base, DirectionalResourceKind Kind>
class DirectionalExtensionForm : public Base {
    public:
        /// @brief This form's directional resource kind, readable at compile time
        /// (@c directional_resource_kind_of_v).
        static constexpr DirectionalResourceKind kind = Kind;

        /// @return @ref kind.
        [[nodiscard]] DirectionalResourceKind directional_resource_kind() const final {
            return kind;
        }
};

}  // namespace rcspp
