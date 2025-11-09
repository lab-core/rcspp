// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <tuple>

namespace rcspp {

template <typename T>
class NumResource;

// ResourceType to ResourceTypeIndex
template <typename ResourceType>
struct ResourceTypeIndex;
template <typename T>
struct ResourceTypeIndex<NumResource<T>> {
        static constexpr std::size_t value = 0;
};
template <typename ResourceType>
constexpr std::size_t ResourceTypeIndex_v = ResourceTypeIndex<ResourceType>::value;

// ResourceType to ResourceInitializerTypeTuple
template <typename ResourceType>
struct ResourceInitializerTypeTuple;
template <typename T>
struct ResourceInitializerTypeTuple<NumResource<T>> {
        using type = std::tuple<double>;
};
template <typename ResourceType>
using ResourceInitializerTypeTuple_t = typename ResourceInitializerTypeTuple<ResourceType>::type;
}  // namespace rcspp
