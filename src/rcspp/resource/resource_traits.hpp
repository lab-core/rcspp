// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <tuple>

namespace rcspp {

class RealResource;
class RealResourceFactory;

// ResourceType to ResourceTypeIndex
template <typename ResourceType>
struct ResourceTypeIndex;
template <>
struct ResourceTypeIndex<RealResource> {
        static constexpr std::size_t value = 0;
};
template <typename ResourceType>
constexpr std::size_t ResourceTypeIndex_v = ResourceTypeIndex<ResourceType>::value;

// ResourceType to ResourceFactoryType
template <typename ResourceType>
struct ResourceFactoryType;
template <>
struct ResourceFactoryType<RealResource> {
        using type = RealResourceFactory;
};
template <typename ResourceType>
using ResourceFactoryType_t = typename ResourceFactoryType<ResourceType>::type;

// ResourceType to ResourceInitializerTypeTuple
template <typename ResourceType>
struct ResourceInitializerTypeTuple;
template <>
struct ResourceInitializerTypeTuple<RealResource> {
        using type = std::tuple<double>;
};
template <typename ResourceType>
using ResourceInitializerTypeTuple_t = typename ResourceInitializerTypeTuple<ResourceType>::type;
}  // namespace rcspp