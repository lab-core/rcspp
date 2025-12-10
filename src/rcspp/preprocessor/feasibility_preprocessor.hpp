// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "rcspp/preprocessor/preprocessor.hpp"

namespace rcspp {

template <typename ResourceType>
class FeasibilityPreprocessor final : public Preprocessor<ResourceType> {
    public:
        FeasibilityPreprocessor(ResourceFactory<ResourceType>* resource_factory,
                                Graph<ResourceType>* graph)
            : Preprocessor<ResourceType>(graph), resource_factory_(resource_factory) {
            for (size_t node_id : graph->get_node_ids()) {
                // Create an initial resource by extending a resource to this node
                const auto* node = graph->get_node(node_id);
                // if source, add default resource
                if (node->source) {
                    initial_resources_by_node_id_[node_id].emplace_back(
                        resource_factory->make_resource(node_id));
                    continue;
                }
                // loop through the in arcs to find all feasible initial resource
                auto& initial_resources = initial_resources_by_node_id_[node_id];
                for (auto* arc : node->in_arcs) {
                    auto previous_resource = resource_factory->make_resource(arc->origin->id);
                    auto new_resource = resource_factory->make_resource(node_id);
                    arc->extender->extend(*previous_resource, new_resource.get());

                    // find the smallest feasible resource
                    if (new_resource->is_feasible()) {
                        initial_resources.emplace_back(std::move(new_resource));
                    }
                }
            }
        }

    private:
        ResourceFactory<ResourceType>* resource_factory_;
        std::map<size_t, std::vector<std::unique_ptr<Resource<ResourceType>>>>
            initial_resources_by_node_id_;
        bool remove_arc(const Arc<ResourceType>& arc) override {
            // extend the initial resources
            auto extended_resource = resource_factory_->make_resource(arc.destination->id);
            for (auto& resource : initial_resources_by_node_id_[arc.origin->id]) {
                arc.extender->extend(*resource, extended_resource.get());
                if (extended_resource->is_feasible()) {
                    // found a feasible extension, do not remove the arc
                    return false;
                }
            }

            // no feasible extension found, remove the arc
            return true;
        }
};
}  // namespace rcspp
