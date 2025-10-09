// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/expander.hpp"
#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/concrete/real_resource.hpp"
#include "rcspp/resource/functions/expansion/expansion_function.hpp"

class RealAdditionExpansionFunction
    : public Clonable<RealAdditionExpansionFunction, ExpansionFunction<RealResource>> {
    public:
        void expand(const Resource<RealResource>& resource, const Expander<RealResource>& expander,
                    Resource<RealResource>& expanded_resource) override;
};
