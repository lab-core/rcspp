// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>

#include "rcspp/resource/base/resource_factory.hpp"
#include "rcspp/resource/concrete/real_resource.hpp"
#include "rcspp/resource/functions/cost/cost_function.hpp"
#include "rcspp/resource/functions/dominance/dominance_function.hpp"
#include "rcspp/resource/functions/expansion/expansion_function.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

class RealResourceFactory : public ResourceFactory<RealResource> {
    public:
        RealResourceFactory();

        explicit RealResourceFactory(const RealResource& real_resource_prototype);

        RealResourceFactory(std::unique_ptr<ExpansionFunction<RealResource>> expansion_function,
                            std::unique_ptr<FeasibilityFunction<RealResource>> feasibility_function,
                            std::unique_ptr<CostFunction<RealResource>> cost_function,
                            std::unique_ptr<DominanceFunction<RealResource>> dominance_function);

        RealResourceFactory(const RealResource& real_resource_prototype,
                            std::unique_ptr<ExpansionFunction<RealResource>> expansion_function,
                            std::unique_ptr<FeasibilityFunction<RealResource>> feasibility_function,
                            std::unique_ptr<CostFunction<RealResource>> cost_function,
                            std::unique_ptr<DominanceFunction<RealResource>> dominance_function);

        // TODO(patrick): Redefine the methods below.

        /*std::unique_ptr<RealResource> make_resource();

        std::unique_ptr<RealResource> make_resource(double value);

        std::unique_ptr<RealResource> make_resource(double value, double min, double max);*/
};
}  // namespace rcspp