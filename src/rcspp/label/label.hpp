// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <iostream>
#include <memory>
#include <utility>

#include "rcspp/graph/arc.hpp"
#include "rcspp/graph/node.hpp"
#include "rcspp/resource/base/resource.hpp"

namespace rcspp {

template <typename ResourceType>
class LabelFactory;

template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class Label {
        friend class LabelFactory<ResourceType>;

    public:
        // Label ID
        size_t id;

        Label(size_t label_id, std::unique_ptr<Resource<ResourceType>> resource)
            : id(label_id),
              dominated(false),
              resource_(std::move(resource)),
              end_node_(nullptr),
              in_arc_(nullptr),
              out_arc_(nullptr) {}

        Label(size_t label_id, std::unique_ptr<Resource<ResourceType>> resource,
              const Node<ResourceType>* end_node, const Arc<ResourceType>* in_arc,
              const Arc<ResourceType>* out_arc)
            : id(label_id),
              dominated(false),
              resource_(std::move(resource)),
              end_node_(end_node),
              in_arc_(in_arc),
              out_arc_(out_arc) {}

        // Check dominance
        [[nodiscard]] bool operator<=(const Label& rhs_label) const {
            return *resource_ <= *rhs_label.resource_;
        }

        // Label expansion
        void expand(const Arc<ResourceType>& arc, Label* expanded_label) const {
            arc.expander->expand(*resource_, *expanded_label->resource_);
            expanded_label->end_node_ = arc.destination;
            expanded_label->in_arc_ = &arc;
            expanded_label->out_arc_ = nullptr;
        }

        // Return label cost
        [[nodiscard]] double get_cost() const { return resource_->get_cost(); }

        // Return true if the label is feasible
        [[nodiscard]] bool is_feasible() const { return resource_->is_feasible(); }

        [[nodiscard]] Resource<ResourceType>& get_resource() const { return *resource_; }

        [[nodiscard]] const Node<ResourceType>* get_end_node() const { return end_node_; }

        [[nodiscard]] const Arc<ResourceType>* get_in_arc() const { return in_arc_; }

        bool dominated;

    private:
        // Resource consumed by the label.
        std::unique_ptr<Resource<ResourceType>> resource_;

        // Pointer to the node at the end of the path associated with the current label.
        const Node<ResourceType>* end_node_;

        // Pointer to the arc from which this label was forward expanded.
        const Arc<ResourceType>* in_arc_;

        // Pointer to the arc from which this label was backward expanded.
        const Arc<ResourceType>* out_arc_;
};
}  // namespace rcspp
