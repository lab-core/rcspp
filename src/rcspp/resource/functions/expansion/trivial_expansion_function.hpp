// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/expander.hpp"
#include "rcspp/resource/functions/expansion/expansion_function.hpp"

namespace rcspp {

template <typename ResourceType>
class TrivialExpansionFunction
    : public Clonable<TrivialExpansionFunction<ResourceType>, ExpansionFunction<ResourceType>> {
    public:
        void expand(const Resource<ResourceType>& resource, const Expander<ResourceType>& expander,
                    Resource<ResourceType>* reused_resource) override {}
};
}  // namespace rcspp
