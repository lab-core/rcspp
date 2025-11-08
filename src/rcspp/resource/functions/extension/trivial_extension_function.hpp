// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename ResourceType>
class TrivialExpansionFunction
    : public Clonable<TrivialExpansionFunction<ResourceType>, ExpansionFunction<ResourceType>> {
    public:
        void extend(const Resource<ResourceType>& resource, const Extender<ResourceType>& extender,
                    Resource<ResourceType>* reused_resource) override {}
};
}  // namespace rcspp
