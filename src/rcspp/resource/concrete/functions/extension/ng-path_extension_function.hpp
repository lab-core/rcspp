// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>
#include <set>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename ResourceType,
          typename ValueType =
              std::decay_t<decltype(std::declval<Resource<ResourceType>>().get_value())>>
class NgPathExtensionFunction : public Clonable<NgPathExtensionFunction<ResourceType, ValueType>,
                                                ExtensionFunction<ResourceType>> {
    public:
        explicit NgPathExtensionFunction(
            const std::map<size_t, std::set<ValueType>>& ng_neighborhood_by_origin_id)
            : ng_neighborhood_by_origin_id_(ng_neighborhood_by_origin_id) {}

        void extend(const Resource<ResourceType>& resource, const Extender<ResourceType>& extender,
                    Resource<ResourceType>* extended_resource) override {
            extend(resource, extender, extended_resource, ng_neighborhood_.get_value());
        }

        void extend_back(const Resource<ResourceType>& resource,
                         const Extender<ResourceType>& extender,
                         Resource<ResourceType>* extended_resource) override {
            extend(resource, extender, extended_resource, ng_neighborhood_back_.get_value());
        }

        void extend(const Resource<ResourceType>& resource, const Extender<ResourceType>& extender,
                    Resource<ResourceType>* extended_resource,
                    const ResourceType& ng_neighborhood) {
            // keep only the nodes in the neighborhood of the origin node of the arc
            auto intersection_container = resource.get_intersection(ng_neighborhood.get_value());
            // then, add the extender value (which is the origin node of the arc normally)
            intersection_container = extender.get_union(intersection_container);
            extended_resource->set_value(intersection_container);
        }

    private:
        // neighborhood of the origin node of the arc
        const std::map<size_t, std::set<ValueType>>& ng_neighborhood_by_origin_id_;
        ResourceType ng_neighborhood_;
        ResourceType ng_neighborhood_back_;

        void preprocess(size_t origin_id, size_t destination_id) override {
            ng_neighborhood_.set_value(ng_neighborhood_by_origin_id_.at(origin_id));
            ng_neighborhood_back_.set_value(ng_neighborhood_by_origin_id_.at(destination_id));
        }
};

}  // namespace rcspp
