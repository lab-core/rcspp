// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <set>
#include <tuple>
#include <type_traits>
#include <utility>

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
}  // namespace rcspp
