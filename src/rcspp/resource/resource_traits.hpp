// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <set>
#include <tuple>
#include <type_traits>
#include <utility>

namespace rcspp {

template <typename T>
class NumResource;

template <typename T>
class ContainerResource;

// ResourceType to ResourceTypeIndex
template <typename ResourceType>
struct ResourceTypeIndex;

// Helper macro to define ResourceTypeIndex for a concrete (non-template) type
#define RCSPP_DEFINE_RESOURCE_INDEX(NAME, TYPE, IDX)    \
    using NAME = TYPE;                                  \
    template <>                                         \
    struct ResourceTypeIndex<TYPE> {                    \
            static constexpr std::size_t value = (IDX); \
    };

// Type aliases for common numeric resource types
RCSPP_DEFINE_RESOURCE_INDEX(RealResource, NumResource<double>, 0)
RCSPP_DEFINE_RESOURCE_INDEX(IntResource, NumResource<int>, 1)
// RCSPP_DEFINE_RESOURCE_INDEX(LongResource, NumResource<int64_t>, 2)
// RCSPP_DEFINE_RESOURCE_INDEX(SizeTResource, NumResource<size_t>, 3)
RCSPP_DEFINE_RESOURCE_INDEX(IntSetResource, ContainerResource<std::set<int>>, 2)

// Example usage:
//   // for a concrete resource type:
//   RCSPP_DEFINE_RESOURCE_INDEX(MyResource, 1)
//
// For template resource patterns (like NumResource<T>) we provide explicit
// specializations below (no macro required).

// Explicit specialization for NumResource<T> is already provided above.

template <typename ResourceType>
constexpr std::size_t ResourceTypeIndex_v = ResourceTypeIndex<ResourceType>::value;

// ResourceType to ResourceInitializerTypeTuple
// Extracts the initializer type tuple for a given ResourceType
// Default implementation deduces the value type from ResourceType::get_value().
template <typename ResourceType>
using ResourceInitializerTypeTuple_t =
    std::tuple<std::decay_t<decltype(std::declval<ResourceType>().get_value())>>;

}  // namespace rcspp
