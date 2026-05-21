// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename ContainerResourceType,
          typename ValueType =
              std::decay_t<decltype(std::declval<Resource<ContainerResourceType>>().get_value())>>
class NgPathExtensionFunction
    : public Clonable<NgPathExtensionFunction<ContainerResourceType, ValueType>,
                      ExtensionFunction<ContainerResourceType>> {
    public:
        explicit NgPathExtensionFunction(
            std::map<size_t, std::set<ValueType>> ng_neighborhood_by_origin_id)
            : ng_neighborhood_by_origin_id_(std::make_shared<const std::map<size_t, ValueType>>(
                  std::move(ng_neighborhood_by_origin_id))) {}

        void extend(const Resource<ContainerResourceType>& resource,
                    const Extender<ContainerResourceType>& extender,
                    Resource<ContainerResourceType>* extended_resource) override {
            // keep only the nodes in the neighborhood of the origin node of the arc
            auto intersection_container = resource.get_intersection(ng_neighborhood_.get_value());
            // then, add the extender value (which is the origin node of the arc normally)
            intersection_container = extender.get_union(intersection_container);
            extended_resource->set_value(intersection_container);
        }

    private:
        // neighborhood of the origin node of the arc
        std::shared_ptr<const std::map<size_t, ValueType>> ng_neighborhood_by_origin_id_;
        ContainerResourceType ng_neighborhood_;

        void preprocess(size_t origin_id, size_t /* destination_id */) override {
            if (ng_neighborhood_by_origin_id_ == nullptr) {
                return;
            }
            auto it = ng_neighborhood_by_origin_id_->find(origin_id);
            // if found, update ng_neighborhood_
            // else, keep ng_neighborhood_ to it's initial value (empty, i.e., reset the
            // neighborhood)
            if (it == ng_neighborhood_by_origin_id_->end()) {
                ng_neighborhood_.set_value(it->second);
            }
        }
};

}  // namespace rcspp
