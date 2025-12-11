// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <iostream>
#include <memory>
#include <utility>

#include "rcspp/graph/arc.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/base/resource_base.hpp"

namespace rcspp {

template <typename ResourceType, typename ResourceClass = Resource<ResourceType>>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class ResourceFactory {
    public:
        ResourceFactory()
            : nb_resource_bases_created_(0), nb_resources_created_(0), nb_extenders_created_(0) {}

        ResourceFactory(std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                        std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
                        std::unique_ptr<CostFunction<ResourceType>> cost_function,
                        std::unique_ptr<DominanceFunction<ResourceType>> dominance_function,
                        const ResourceType& resource_base_prototype)
            : resource_prototype_(make_resource_prototype(
                  std::move(dominance_function), std::move(feasibility_function),
                  std::move(cost_function), resource_base_prototype)),
              extension_function_(std::move(extension_function)),
              nb_resource_bases_created_(0),
              nb_resources_created_(0),
              nb_extenders_created_(0) {}

        ResourceFactory(std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                        std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
                        std::unique_ptr<CostFunction<ResourceType>> cost_function,
                        std::unique_ptr<DominanceFunction<ResourceType>> dominance_function)
            : resource_prototype_(make_resource_prototype(std::move(dominance_function),
                                                          std::move(feasibility_function),
                                                          std::move(cost_function))),
              extension_function_(std::move(extension_function)),
              nb_resource_bases_created_(0),
              nb_resources_created_(0),
              nb_extenders_created_(0) {}

        /*ResourceFactory(std::unique_ptr<ResourceType> resource_prototype) :
          resource_prototype_(std::move(resource_prototype)), nb_resources_created_(0) {
        }*/

        virtual auto make_resource_base() -> std::unique_ptr<ResourceType> {
            ++nb_resource_bases_created_;
            return resource_prototype_->clone();
        }

        // Make a resource from the prototype.
        virtual auto make_resource() -> std::unique_ptr<ResourceClass> {
            ++nb_resources_created_;
            return resource_prototype_->clone_resource();
        }

        // Make a resource from the prototype with node_id.
        virtual auto make_resource(size_t node_id) -> std::unique_ptr<ResourceClass> {
            ++nb_resources_created_;
            return resource_prototype_->create(node_id);
        }

        // Make a resource from another resource by copying its resource function objects.
        virtual auto make_resource(const ResourceClass& resource)
            -> std::unique_ptr<ResourceClass> {
            ++nb_resources_created_;
            return resource.copy();
        }

        // Make an extender
        template <typename ExtenderClass = Extender<ResourceType>, typename GraphResourceType>
        auto make_extender(const Arc<GraphResourceType>& arc) -> std::unique_ptr<ExtenderClass> {
            ++nb_extenders_created_;
            return std::make_unique<ExtenderClass>(extension_function_->create(arc), arc.id);
        }

        // Make an extender
        // clang-format off
    template <typename ExtenderClass = Extender<ResourceType>, typename GraphResourceType>
         auto make_extender(const ResourceType& resource_base, const Arc<GraphResourceType>& arc)
            -> std::unique_ptr<ExtenderClass> {
            ++nb_extenders_created_;
            return std::make_unique<ExtenderClass>(resource_base,
                                                            extension_function_->create(arc),
                                                            arc.id);
        }
        // clang-format on

    protected:
        // Create a resource prototype with specific functions (but without resource base).
        auto make_resource_prototype(
            std::unique_ptr<DominanceFunction<ResourceType>> dominance_function,
            std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
            std::unique_ptr<CostFunction<ResourceType>> cost_function)
            -> std::unique_ptr<ResourceClass> {
            return std::make_unique<ResourceClass>(std::move(dominance_function),
                                                   std::move(feasibility_function),
                                                   std::move(cost_function));
        }

        // Create a resource prototype with specific functions and a resource base.
        auto make_resource_prototype(
            std::unique_ptr<DominanceFunction<ResourceType>> dominance_function,
            std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
            std::unique_ptr<CostFunction<ResourceType>> cost_function,
            const ResourceType& resource_base_prototype) -> std::unique_ptr<ResourceClass> {
            return std::make_unique<ResourceClass>(std::move(dominance_function),
                                                   std::move(feasibility_function),
                                                   std::move(cost_function),
                                                   resource_base_prototype);
        }

        std::unique_ptr<ResourceClass> resource_prototype_;
        std::unique_ptr<ExtensionFunction<ResourceType>> extension_function_;

        size_t nb_resource_bases_created_;
        size_t nb_resources_created_;
        size_t nb_extenders_created_;
};
}  // namespace rcspp
