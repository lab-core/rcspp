// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <list>

#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/dominance_mask.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp {

/// @brief Flat list of non-dominated labels for a single node.
///
/// Used as the default label container in dominance algorithms.  Every
/// add / erase / dominance-check is O(N) in the number of stored labels.
///
/// Dominance is checked here, so a backward search must use a backward container; a forward one
/// would discard the better labels.
///
/// @tparam ResourceType The resource type carried by labels.
/// @tparam Direction    The direction policy supplying the dominance order.
template <class ResourceType, class Direction = ForwardDirection>
class LabelList {
        using LabelPosition = std::list<Label<ResourceType>*>::iterator;

    public:
        explicit LabelList() = default;
        virtual ~LabelList() = default;

        /// @brief Returns an empty container with the same configuration.
        ///
        /// Inside the class, `LabelList` names this exact instantiation, so the copy keeps
        /// @c Direction.
        [[nodiscard]] LabelList copy() const { return LabelList(); }

        /// @brief Returns an empty list that compares labels in direction @p TargetDirection.
        ///
        /// A search in another direction keeps its labels in this copy (see
        /// @c detail::make_directed_container).
        ///
        /// @tparam TargetDirection The direction policy of the copy.
        template <class TargetDirection>
        [[nodiscard]] LabelList<ResourceType, TargetDirection> with_direction() const {
            return LabelList<ResourceType, TargetDirection>();
        }

        /// @brief Read-only access to the underlying label list.
        [[nodiscard]] const std::list<Label<ResourceType>*>& get_labels() const { return labels_; }

        /// @brief Appends @p label at the end and returns its iterator.
        virtual LabelPosition add_label(Label<ResourceType>* label) {
            return labels_.insert(labels_.end(), label);
        }

        /// @brief Removes the label at position @p pos.
        virtual void erase_label(const LabelPosition& pos) { labels_.erase(pos); }

        /// @brief Logs all stored labels at TRACE level.
        virtual void print_labels() const {
            if (LOG_TRACE_ACTIVE()) {
                for (auto label_ptr : labels_) {
                    LOG_TRACE("  ", label_ptr, ": ", label_ptr->get_resource().to_string(), "\n");
                }
            }
        }

        /// @brief Marks and removes all labels dominated by @p label.
        /// @return Number of labels removed.
        virtual size_t remove_dominated_labels(const Label<ResourceType>& label) {
            return remove_dominated_labels_with(
                label,
                [](const Label<ResourceType>& lhs, const Label<ResourceType>& rhs) {
                    return dominates(lhs, rhs);
                });
        }

        /// @brief Marks and removes all labels @p label dominates over the components @p mask
        ///        leaves in.
        /// @return Number of labels removed.
        virtual size_t remove_dominated_labels(const Label<ResourceType>& label,
                                               const DominanceMask& mask) {
            return remove_dominated_labels_with(
                label,
                [&mask](const Label<ResourceType>& lhs, const Label<ResourceType>& rhs) {
                    return dominates(lhs, rhs, mask);
                });
        }

        /// @brief Returns true if any stored label dominates @p label.
        [[nodiscard]] virtual bool is_dominated(const Label<ResourceType>& label) const {
            return is_dominated_with(
                label,
                [](const Label<ResourceType>& lhs, const Label<ResourceType>& rhs) {
                    return dominates(lhs, rhs);
                });
        }

        /// @brief Returns true if any stored label dominates @p label over the components
        ///        @p mask leaves in.
        [[nodiscard]] virtual bool is_dominated(const Label<ResourceType>& label,
                                                const DominanceMask& mask) const {
            return is_dominated_with(
                label,
                [&mask](const Label<ResourceType>& lhs, const Label<ResourceType>& rhs) {
                    return dominates(lhs, rhs, mask);
                });
        }

        /// @brief Removes the costliest label, marking it dominated, when it costs more than
        ///        @p cost; of labels of equal cost, the last in the container's order.
        /// @return Whether a label was removed.
        bool remove_costliest_above(double cost) {
            auto costliest = labels_.end();
            for (auto it = labels_.begin(); it != labels_.end(); ++it) {
                if (costliest == labels_.end() || (*it)->get_cost() >= (*costliest)->get_cost()) {
                    costliest = it;
                }
            }
            if (costliest == labels_.end() || (*costliest)->get_cost() <= cost) {
                return false;
            }
            (*costliest)->dominated = true;
            erase_label(costliest);
            return true;
        }

    protected:
        /// @brief Direction-aware dominance: forward uses `operator<=`, backward uses
        ///        `back_dominates`.
        ///
        /// @param lhs The candidate dominating label.
        /// @param rhs The label being tested.
        /// @return `true` when @p lhs dominates @p rhs in this container's direction.
        [[nodiscard]] static bool dominates(const Label<ResourceType>& lhs,
                                            const Label<ResourceType>& rhs) {
            return Direction::template dominates<ResourceType>(lhs, rhs);
        }

        /// @brief Dominance over the components @p mask leaves in: each applies its own rule,
        ///        in this container's direction, as `CompositionDominanceFunction` does, whatever
        ///        composition dominance function the model uses. A resource that is not a
        ///        composition has one component.
        ///
        /// @param lhs  The candidate dominating label.
        /// @param rhs  The label being tested.
        /// @param mask The components left out.
        /// @return `true` when every component left in of @p lhs dominates that of @p rhs.
        [[nodiscard]] static bool dominates(const Label<ResourceType>& lhs,
                                            const Label<ResourceType>& rhs,
                                            const DominanceMask& mask) {
            if constexpr (requires { lhs.get_resource().get_components(); }) {
                size_t component = 0;
                return lhs.get_resource().for_each_component_and(
                    rhs.get_resource(),
                    [&component, &mask](const auto& lhs_component, const auto& rhs_component) {
                        if (mask.ignores(component++)) {
                            return true;
                        }
                        if constexpr (Direction::backward) {
                            return lhs_component.back_dominates(rhs_component);
                        } else {
                            return lhs_component <= rhs_component;
                        }
                    });
            } else {
                return mask.ignores(0) || dominates(lhs, rhs);
            }
        }

        /// @brief @ref remove_dominated_labels, with @p dominates the test.
        template <typename DominatesCallable>
        size_t remove_dominated_labels_with(const Label<ResourceType>& label,
                                            const DominatesCallable& dominates) {
            size_t removed = 0;
            for (auto non_dominated_label_it = labels_.begin();
                 non_dominated_label_it != labels_.end();) {
                if (&label != *non_dominated_label_it &&
                    dominates(label, *(*non_dominated_label_it))) {
                    (*non_dominated_label_it)->dominated = true;
                    non_dominated_label_it = labels_.erase(non_dominated_label_it);
                    ++removed;
                } else {
                    ++non_dominated_label_it;
                }
            }
            return removed;
        }

        /// @brief @ref is_dominated, with @p dominates the test.
        template <typename DominatesCallable>
        [[nodiscard]] bool is_dominated_with(const Label<ResourceType>& label,
                                             const DominatesCallable& dominates) const {
            for (const auto non_dominated_label_ptr : labels_) {
                if (&label == non_dominated_label_ptr) {
                    continue;
                }
                if (dominates(*non_dominated_label_ptr, label)) {
                    return true;
                }
            }
            return false;
        }

        std::list<Label<ResourceType>*> labels_;
};

}  // namespace rcspp
