// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <set>
#include <tuple>
#include <type_traits>
#include <utility>

#include "rcspp/resource/composition/composition.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"

namespace rcspp {

/* Definitions of different useful ResourceType */
// Specialization for NumericalResource
template <typename T>
class NumericalResource;

using RealResource = NumericalResource<double>;
using IntResource = NumericalResource<int>;
using UIntResource = NumericalResource<unsigned int>;

// Specialization for SetResource
template <typename T>
class SetResource;
using RealSetResource = SetResource<double>;
using IntSetResource = SetResource<int>;
using UIntSetResource = SetResource<unsigned int>;
using SizeTSetResource = SetResource<size_t>;

template <typename T>
class BitsetResource;
// specialization for BitsetResource<T>
template <typename T>
struct ComponentInitializerTypeTuple<BitsetResource<T>> {
        using type = std::tuple<std::set<T>>;
};

using UIntBitsetResource = BitsetResource<unsigned int>;
using SizeTBitsetResource = BitsetResource<size_t>;

// Type trait: true iff T is NumericalResource<U> for some U
template <typename T>
struct is_numerical_resource : std::false_type {};
template <typename T>
struct is_numerical_resource<NumericalResource<T>> : std::true_type {};
template <typename T>
inline constexpr bool is_numerical_resource_v = is_numerical_resource<T>::value;

// ── Helper: true when CostResourceType is a component of CompositionType ─────
template <typename CostResourceType, typename CompositionType>
struct is_cost_in_composition : std::false_type {};
template <typename CostResourceType, typename... ResourceTypes>
struct is_cost_in_composition<CostResourceType, ResourceTypeComposition<ResourceTypes...>>
    : std::bool_constant<ComponentTypeIndex<CostResourceType, ResourceTypes...>::value != -1> {};
template <typename CostResourceType, typename CompositionType>
inline constexpr bool is_cost_in_composition_v =
    is_cost_in_composition<CostResourceType, CompositionType>::value;
}  // namespace rcspp
