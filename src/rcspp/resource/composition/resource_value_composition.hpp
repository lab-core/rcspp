// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/resource/base/resource_value.hpp"

namespace rcspp {

// Just a placeholder to store the template types
template <typename... ResourceTypes>
class ResourceValueComposition : public ResourceValue<ResourceValueComposition<ResourceTypes...>> {
    public:
        ResourceValueComposition() = default;

        void reset() override {}
};

}  // namespace rcspp
