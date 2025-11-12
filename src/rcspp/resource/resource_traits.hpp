// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <set>
#include <tuple>
#include <type_traits>
#include <utility>

namespace rcspp {
// compute index of first type in ResourceTypes... that is ResourceType
// Base template
template <typename ResourceType, typename... ResourceTypes>
struct ResourceTypeIndex;

// default case: not found
template <typename ResourceType>
struct ResourceTypeIndex<ResourceType> {
        static constexpr int value = -1;
};

// recursive case: either found at current index (ResourceType == ResourceType1)
// or continue searching in ResourceTypes...
template <typename ResourceType, typename ResourceType1, typename... ResourceTypes>
struct ResourceTypeIndex<ResourceType, ResourceType1, ResourceTypes...> {
    private:
        static constexpr int next = ResourceTypeIndex<ResourceType, ResourceTypes...>::value;
        static constexpr int next_or_minus_one = (next == -1 ? -1 : 1 + next);

    public:
        static constexpr int value =
            std::is_same_v<ResourceType, ResourceType1> ? 0 : next_or_minus_one;
};

// Retrieve the value of the associated template
template <typename ResourceType, typename... ResourceTypes>
    requires(ResourceTypeIndex<ResourceType, ResourceTypes...>::value != -1)
inline constexpr int ResourceTypeIndex_v = ResourceTypeIndex<ResourceType, ResourceTypes...>::value;

// ResourceType to ResourceInitializerTypeTuple
// Extracts the initializer type tuple for a given ResourceType
// Default implementation deduces the value type from ResourceType::get_value().
// Specializations can be provided for more complex ResourceTypes that have initializer with several
// values
// default trait (can be specialized)
template <typename ResourceType>
struct ResourceInitializerTypeTuple {
        using type = std::tuple<std::decay_t<decltype(std::declval<ResourceType>().get_value())>>;
};

// convenience alias
template <typename ResourceType>
using ResourceInitializerTypeTuple_t = typename ResourceInitializerTypeTuple<ResourceType>::type;

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

template <typename T>
class BitsetResource;
// specialization for BitsetResource<T>
template <typename T>
struct ResourceInitializerTypeTuple<BitsetResource<T>> {
        using type = std::tuple<std::set<T>>;
};

using UIntBitsetResource = BitsetResource<unsigned int>;
using SizeTBitsetResource = BitsetResource<size_t>;
}  // namespace rcspp
