// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/concrete/real_resource.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

class RealAdditionExtensionFunction
    : public Clonable<RealAdditionExtensionFunction, ExpansionFunction<RealResource>> {
    public:
        void extend(const Resource<RealResource>& resource, const Extender<RealResource>& extender,
                    Resource<RealResource>* extended_resource) override;
};
}  // namespace rcspp
