// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <iostream>
#include <memory>
#include <tuple>
#include <utility>

#include "rcspp/graph/arc.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/base/resource_type.hpp"

namespace rcspp {

template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class ResourceFactory {
        using ResourceClass = Resource<ResourceType>;
        using ExtenderClass = Extender<ResourceType>;

    public:
        ResourceFactory()
            : nb_resource_bases_created_(0), nb_resources_created_(0), nb_extenders_created_(0) {}

        ResourceFactory(std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                        std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
                        std::unique_ptr<CostFunction<ResourceType>> cost_function,
                        std::unique_ptr<DominanceFunction<ResourceType>> dominance_function,
                        const ResourceType& resource_value_prototype)
            : resource_prototype_(create_resource_prototype(
                  std::move(dominance_function), std::move(feasibility_function),
                  std::move(cost_function), resource_value_prototype)),
              extension_function_(std::move(extension_function)),
              nb_resource_bases_created_(0),
              nb_resources_created_(0),
              nb_extenders_created_(0) {}

        ResourceFactory(std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                        std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
                        std::unique_ptr<CostFunction<ResourceType>> cost_function,
                        std::unique_ptr<DominanceFunction<ResourceType>> dominance_function)
            : resource_prototype_(create_resource_prototype(std::move(dominance_function),
                                                            std::move(feasibility_function),
                                                            std::move(cost_function))),
              extension_function_(std::move(extension_function)),
              nb_resource_bases_created_(0),
              nb_resources_created_(0),
              nb_extenders_created_(0) {}

        // Make a resource from the prototype.
        virtual auto create_resource() -> std::unique_ptr<ResourceClass> {
            ++nb_resources_created_;
            return resource_prototype_->clone();
        }

        // Make a resource from the prototype with node_id.
        virtual auto create_resource(size_t node_id) -> std::unique_ptr<ResourceClass> {
            ++nb_resources_created_;
            return resource_prototype_->create(node_id);
        }

        virtual auto create_resource(size_t node_id, const ResourceType& resource_base)
            -> std::unique_ptr<ResourceClass> {
            ++nb_resources_created_;
            return resource_prototype_->create(resource_base, node_id);
        }

        // Make a resource from another resource by copying its resource function objects.
        virtual auto copy_resource(const ResourceClass& resource)
            -> std::unique_ptr<ResourceClass> {
            ++nb_resources_created_;
            return resource.copy();
        }

        // Make a resource from a node by copying its resource object.
        virtual auto copy_resource(const Node<ResourceType>& node)
            -> std::unique_ptr<ResourceClass> {
            return copy_resource(*node.resource);
        }

        // Make a resource from the prototype, initialized from an initializer tuple.
        template <typename... Args>
        auto create_resource(const std::tuple<Args...>& resource_initializer)
            -> std::unique_ptr<ResourceClass> {
            ++nb_resources_created_;
            auto new_resource = resource_prototype_->clone();
            std::apply(
                [&new_resource](auto&&... args) {
                    new_resource->set_value(std::forward<decltype(args)>(args)...);
                },
                resource_initializer);
            return new_resource;
        }

        /// @brief Return a deep copy of this factory (prototype + extension function).
        [[nodiscard]] virtual std::unique_ptr<ResourceFactory<ResourceType>> clone() const {
            auto cloned = std::make_unique<ResourceFactory<ResourceType>>();
            if (resource_prototype_) {
                cloned->resource_prototype_ = resource_prototype_->clone();
            }
            if (extension_function_) {
                cloned->extension_function_ = extension_function_->clone();
            }
            return cloned;
        }

        // Make an extender
        template <typename GraphResourceType>
        auto create_extender(const Arc<GraphResourceType>& arc) -> std::unique_ptr<ExtenderClass> {
            ++nb_extenders_created_;
            return std::make_unique<ExtenderClass>(extension_function_->create(arc), arc.id);
        }

        // Make an extender
        // clang-format off
    template <typename GraphResourceType>
         auto create_extender(const ResourceType& resource_value, const Arc<GraphResourceType>& arc)
            -> std::unique_ptr<ExtenderClass> {
            ++nb_extenders_created_;
            return std::make_unique<ExtenderClass>(resource_value,
                                                            extension_function_->create(arc),
                                                            arc.id);
        }
        // clang-format on

        template <typename... Args, typename GraphResourceType>
        auto create_extender(const std::tuple<Args...>& resource_initializer,
                             const Arc<GraphResourceType>& arc) -> std::unique_ptr<ExtenderClass> {
            ++nb_extenders_created_;
            return std::make_unique<ExtenderClass>(
                std::apply(
                    [](auto&&... args) {  // unpack arguments
                        return ResourceType(std::forward<decltype(args)>(args)...);
                    },
                    resource_initializer),
                extension_function_->create(arc),
                arc.id);
        }

    protected:
        // Create a resource prototype with specific functions (but without resource base).
        auto create_resource_prototype(
            std::unique_ptr<DominanceFunction<ResourceType>> dominance_function,
            std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
            std::unique_ptr<CostFunction<ResourceType>> cost_function)
            -> std::unique_ptr<ResourceClass> {
            return std::make_unique<ResourceClass>(std::move(dominance_function),
                                                   std::move(feasibility_function),
                                                   std::move(cost_function));
        }

        // Create a resource prototype with specific functions and a resource base.
        auto create_resource_prototype(
            std::unique_ptr<DominanceFunction<ResourceType>> dominance_function,
            std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
            std::unique_ptr<CostFunction<ResourceType>> cost_function,
            const ResourceType& resource_value_prototype) -> std::unique_ptr<ResourceClass> {
            return std::make_unique<ResourceClass>(resource_value_prototype,
                                                   std::move(dominance_function),
                                                   std::move(feasibility_function),
                                                   std::move(cost_function));
        }

        std::unique_ptr<ResourceClass> resource_prototype_;
        std::unique_ptr<ExtensionFunction<ResourceType>> extension_function_;

        size_t nb_resource_bases_created_;
        size_t nb_resources_created_;
        size_t nb_extenders_created_;
};
}  // namespace rcspp
