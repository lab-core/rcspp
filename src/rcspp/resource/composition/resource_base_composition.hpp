// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/resource/base/resource_base.hpp"

namespace rcspp {

// Just a placeholder to store the template types
template <typename... ResourceTypes>
class ResourceBaseComposition : public ResourceBase<ResourceBaseComposition<ResourceTypes...>> {
    public:
        ResourceBaseComposition() = default;

        void reset() override {}
};
}  // namespace rcspp
