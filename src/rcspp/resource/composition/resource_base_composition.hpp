// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <concepts>  // NOLINT(build/include_order)
#include <iterator>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/resource/base/resource_base.hpp"
#include "rcspp/resource/composition/composition.hpp"

namespace rcspp {

template <typename... ResourceTypes>
class ResourceBaseComposition : public ResourceBase<ResourceBaseComposition<ResourceTypes...>> {
        template <typename... Types>
        friend class ResourceBaseCompositionFactory;

    public:
        ResourceBaseComposition() = default;

        void reset() override {}
};
}  // namespace rcspp
