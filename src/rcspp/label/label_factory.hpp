// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <utility>

#include "rcspp/label/label.hpp"
#include "rcspp/resource/base/resource_factory.hpp"

namespace rcspp {

template <typename ResourceType>
// requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class LabelFactory {
    public:
        explicit LabelFactory(ResourceFactory<ResourceType>* resource_factory)
            : resource_factory_(*resource_factory) {}

        std::unique_ptr<Label<ResourceType>> make_label(
            size_t label_id, const Node<ResourceType>* end_node,
            const Arc<ResourceType>* in_arc = nullptr, const Arc<ResourceType>* out_arc = nullptr) {
            auto resource = resource_factory_.make_resource(*end_node->resource);

            return std::make_unique<Label<ResourceType>>(label_id,
                                                         std::move(resource),
                                                         end_node,
                                                         in_arc,
                                                         out_arc);
        }

        static void reset_label(Label<ResourceType>* label, size_t label_id,
                                const Node<ResourceType>* end_node,
                                const Arc<ResourceType>* in_arc = nullptr,
                                const Arc<ResourceType>* out_arc = nullptr) {
            label->id = label_id;
            label->end_node_ = end_node;
            label->in_arc_ = in_arc;
            label->out_arc_ = out_arc;
            label->dominated = false;

            label->get_resource().reset(*end_node->resource);
        }

    private:
        ResourceFactory<ResourceType>& resource_factory_;
};
}  // namespace rcspp
