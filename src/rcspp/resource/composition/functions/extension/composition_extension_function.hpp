// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <iostream>
#include <tuple>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename... ResourceTypes>
class CompositionExtensionFunction
    : public Clonable<CompositionExtensionFunction<ResourceTypes...>,
                      ExtensionFunction<ResourceComposition<ResourceTypes...>>> {
    public:
        void extend(const Resource<ResourceComposition<ResourceTypes...>>& resource,
                    const Extender<ResourceComposition<ResourceTypes...>>& extender,
                    Resource<ResourceComposition<ResourceTypes...>>* extended_resource) override {
            auto& resource_components = resource.get_resource_components();
            auto& extender_components = extender.get_extender_components();
            auto& extended_resource_components = extended_resource->get_resource_components();

            /*const auto extend_res_function = [&](const auto& sing_res_vec, const auto&
              sing_exp_vec, const auto& extended_sing_res_vec) {

              for (int i = 0; i < sing_res_vec.size(); i++) {
                sing_exp_vec[i]->extend(*sing_res_vec[i], *extended_sing_res_vec[i]);
              }
              };*/

            /*std::apply([&](auto && ... args_res) {
              std::apply([&](auto && ... args_exp) {
                std::apply([&](auto && ... args_extended) {

                  (extend_res_function(args_res, args_exp, args_extended), ...);

                  }, extended_resource_components);
                }, extender_components);
              }, resource_components);*/

            std::apply(
                [&](auto&&... args_res) {
                    std::apply(
                        [&](auto&&... args_exp) {
                            std::apply(
                                [&](auto&&... args_extended) {
                                    (extend_resource(args_res, args_exp, args_extended), ...);
                                },
                                extended_resource_components);
                        },
                        extender_components);
                },
                resource_components);
        }

    private:
        void extend_resource(const auto& sing_res_vec, const auto& sing_exp_vec,
                             const auto& extended_sing_res_vec) const {
            for (int i = 0; i < sing_res_vec.size(); i++) {
                sing_exp_vec[i]->extend(*sing_res_vec[i], extended_sing_res_vec[i].get());
            }
        }
};
}  // namespace rcspp
