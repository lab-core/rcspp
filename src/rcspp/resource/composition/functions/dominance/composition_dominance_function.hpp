// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/dominance/dominance_function.hpp"

// TODO(patrick): Define dominance_res_function as a method.

namespace rcspp {

template <typename... ResourceTypes>
class CompositionDominanceFunction
    : public Clonable<CompositionDominanceFunction<ResourceTypes...>,
                      DominanceFunction<ResourceBaseComposition<ResourceTypes...>>> {
    public:
        CompositionDominanceFunction() = default;

        [[nodiscard]] bool check_dominance(
            const Resource<ResourceBaseComposition<ResourceTypes...>>& lhs_composition,
            const Resource<ResourceBaseComposition<ResourceTypes...>>& rhs_composition) override {
            return lhs_composition.apply_and(
                rhs_composition,
                [&](const auto& lhs_sing_res, const auto& rhs_sing_res) {
                    for (int i = 0; i < lhs_sing_res.size(); i++) {
                        if (!(*lhs_sing_res[i] <= *rhs_sing_res[i])) {
                            return false;
                        }
                    }
                    return true;
                });
        }
};
}  // namespace rcspp
