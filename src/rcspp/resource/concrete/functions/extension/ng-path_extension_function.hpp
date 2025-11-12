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
            // keep only the nodes in the neighborhood of the origin node of the arc
            auto intersection_container = resource.get_intersection(ng_neighborhood_.get_value());
            // then, add the extender value (which is the origin node of the arc normally)
            intersection_container = extender.get_union(intersection_container);
            extended_resource->set_value(intersection_container);
        }

    private:
        // neighborhood of the origin node of the arc
        const std::map<size_t, std::set<ValueType>>& ng_neighborhood_by_origin_id_;
        ResourceType ng_neighborhood_;

        template <typename GraphResourceType>
        void preprocess(const Arc<GraphResourceType>& arc) {
            ng_neighborhood_.set_value(ng_neighborhood_by_origin_id_.at(arc.origin->id));
        }
};

}  // namespace rcspp
