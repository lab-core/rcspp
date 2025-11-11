// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>
#include <set>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {
template <typename ResourceType, typename ValueType>
class NgPathExpansionFunction : public Clonable<NgPathExpansionFunction<ResourceType, ValueType>,
                                                ExpansionFunction<ResourceType>> {
    public:
        explicit NgPathExpansionFunction(
            const std::map<size_t, std::set<ValueType>>& ng_neighborhood_by_origin_id)
            : ng_neighborhood_by_origin_id_(ng_neighborhood_by_origin_id) {}

        void extend(const Resource<ResourceType>& resource, const Extender<ResourceType>& extender,
                    Resource<ResourceType>* extended_resource) override {
            // keep only the nodes in the neighborhood of the origin node of the arc
            auto intersection_container = resource.get_intersection(ng_neighborhood_);
            extended_resource->set_value(intersection_container);
            // then, add the extender value (which is the origin node of the arc normally)
            intersection_container = extended_resource->get_union(extender.get_value());
            extended_resource->set_value(intersection_container);
        }

    private:
        // neighborhood of the origin node of the arc
        const std::map<size_t, std::set<ValueType>>& ng_neighborhood_by_origin_id_;
        std::set<ValueType> ng_neighborhood_{};

        template <typename GraphResourceType>
        void preprocess(const Arc<GraphResourceType>& arc) {
            ng_neighborhood_ = ng_neighborhood_by_origin_id_.at(arc.origin->id);
        }
};

template <typename ResourceType>
using NgPathExtensionFunction = NgPathExpansionFunction<
    ResourceType, std::decay_t<decltype(std::declval<Resource<ResourceType>>().get_value())>>;
}  // namespace rcspp
