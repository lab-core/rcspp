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
            const ResourceComposition<ResourceTypes...>& lhs_resource,
            const ResourceComposition<ResourceTypes...>& rhs_resource) override {
            return lhs_resource.apply_and(rhs_resource,
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
