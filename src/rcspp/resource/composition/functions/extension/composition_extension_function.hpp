// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <iostream>
#include <tuple>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/extender_composition.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename... ResourceTypes>
class CompositionExtensionFunction
    : public Clonable<CompositionExtensionFunction<ResourceTypes...>,
                      ExtensionFunction<ResourceBaseComposition<ResourceTypes...>>> {
    public:
        void extend(const ResourceComposition<ResourceTypes...>& resource,
                    const ExtenderComposition<ResourceTypes...>& extender,
                    ResourceComposition<ResourceTypes...>* extended_resource) override {
            extend_helper(
                resource,
                extender,
                extended_resource,
                [&](const auto& sing_res_vec,
                    const auto& sing_exp_vec,
                    const auto& extended_sing_res_vec) {
                    this->extend_resource(sing_res_vec, sing_exp_vec, extended_sing_res_vec);
                });
        }

        void extend_back(const ResourceComposition<ResourceTypes...>& resource,
                         const Extender<ResourceBaseComposition<ResourceTypes...>>& extender,
                         ResourceComposition<ResourceTypes...>* extended_resource) override {
            extend_helper(
                resource,
                extender,
                extended_resource,
                [&](const auto& sing_res_vec,
                    const auto& sing_exp_vec,
                    const auto& extended_sing_res_vec) {
                    this->extend_back_resource(sing_res_vec, sing_exp_vec, extended_sing_res_vec);
                });
        }

    private:
        template <typename F>
        void extend_helper(const ResourceComposition<ResourceTypes...>& resource,
                           const Extender<ResourceBaseComposition<ResourceTypes...>>& extender,
                           ResourceComposition<ResourceTypes...>* extended_resource,
                           const F& extend_func) const {
            resource.apply(extender, [&](const auto& sing_res_vec, const auto& sing_exp_vec) {
                std::apply(
                    [&](auto&&... args_extended) {
                        (extend_func(sing_res_vec, sing_exp_vec, args_extended), ...);
                    },
                    extended_resource->get_components());
            });
        }

        void extend_resource(const auto& sing_res_vec, const auto& sing_exp_vec,
                             const auto& extended_sing_res_vec) const {
            const std::size_t n = sing_res_vec.size();
            for (std::size_t i = 0; i < n; ++i) {
                sing_exp_vec[i]->extend(*sing_res_vec[i], extended_sing_res_vec[i].get());
            }
        }

        void extend_back_resource(const auto& sing_res_vec, const auto& sing_exp_vec,
                                  const auto& extended_sing_res_vec) const {
            const std::size_t n = sing_res_vec.size();
            for (std::size_t i = 0; i < n; ++i) {
                sing_exp_vec[i]->extend_back(*sing_res_vec[i], extended_sing_res_vec[i].get());
            }
        }
};
}  // namespace rcspp
