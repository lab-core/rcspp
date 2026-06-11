// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <utility>

#include "rcspp/label/label.hpp"
#include "rcspp/resource/base/resource_factory.hpp"

namespace rcspp {

/// @brief Factory for creating and resetting @c Label objects.
///
/// @c LabelFactory owns a reference to a @c ResourceFactory and uses it to
/// allocate fresh resource states when constructing new labels. It also provides
/// a static helper to reinitialise an existing label in-place, enabling label
/// recycling without heap allocation.
///
/// @tparam ResourceType The resource type used by the labels produced by this factory.
template <typename ResourceType>
class LabelFactory {
    public:
        /// @brief Constructs a @c LabelFactory backed by the given @c ResourceFactory.
        ///
        /// @param resource_factory Pointer to the resource factory used to allocate
        ///                         resource states for new labels. Must outlive this factory.
        explicit LabelFactory(ResourceFactory<ResourceType>* resource_factory)
            : resource_factory_(*resource_factory) {}

        /// @brief Allocates and initialises a new label at the specified graph position.
        ///
        /// A fresh resource state is copied from the end node and wrapped in the new label.
        ///
        /// @param label_id   Numeric identifier to assign to the new label.
        /// @param end_node   Pointer to the node at the end of the partial path.
        /// @param in_arc     Optional pointer to the arc used for the forward extension
        ///                   that produced this label (defaults to @c nullptr).
        /// @param out_arc    Optional pointer to the arc used for the backward extension
        ///                   that produced this label (defaults to @c nullptr).
        /// @return An owning @c unique_ptr to the newly constructed label.
        std::unique_ptr<Label<ResourceType>> make_label(
            size_t label_id, const Node<ResourceType>* end_node,
            const Arc<ResourceType>* in_arc = nullptr, const Arc<ResourceType>* out_arc = nullptr) {
            auto resource = resource_factory_.copy_resource(*end_node);

            return std::make_unique<Label<ResourceType>>(label_id,
                                                         std::move(resource),
                                                         end_node,
                                                         in_arc,
                                                         out_arc);
        }

        /// @brief Resets an existing label to a fresh state at the specified graph position.
        ///
        /// All bookkeeping fields (@c dominated, @c prev_label, @c ref_count,
        /// @c pending_release) are cleared, and the label's resource is reset to the
        /// initial state of @p end_node. This enables label recycling without allocation.
        ///
        /// @param label      Pointer to the label to reset. Must not be @c nullptr.
        /// @param label_id   New numeric identifier to assign to the label.
        /// @param end_node   Pointer to the node at the end of the new partial path.
        /// @param in_arc     Optional pointer to the incoming arc (defaults to @c nullptr).
        /// @param out_arc    Optional pointer to the outgoing arc (defaults to @c nullptr).
        static void reset_label(Label<ResourceType>* label, size_t label_id,
                                const Node<ResourceType>* end_node,
                                const Arc<ResourceType>* in_arc = nullptr,
                                const Arc<ResourceType>* out_arc = nullptr) {
            label->id = label_id;
            label->end_node_ = end_node;
            label->in_arc_ = in_arc;
            label->out_arc_ = out_arc;
            label->dominated = false;
            label->prev_label = nullptr;
            label->ref_count = 0;
            label->pending_release = false;

            label->get_resource().reset(*end_node->resource);
        }

    private:
        ResourceFactory<ResourceType>& resource_factory_;
};
}  // namespace rcspp
