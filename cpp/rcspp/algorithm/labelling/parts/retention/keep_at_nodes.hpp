// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <list>
#include <vector>

#include "rcspp/algorithm/labelling/parts/admission/node_view.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp::parts {

/// @brief Kept labels live in their node's container, where dominance reads them, and the labels
///        at the end nodes are read as the solutions when the loop ends. The retention of every
///        exact search.
///
/// It owns one container per node, of the type the params chose (@c LabelList,
/// @c LabelBuckets), and forwards to it. The strategy chooses the retention (whether labels are
/// stored at the nodes); the user chooses the container (how they are stored).
///
/// A label leaves its container when it is evicted (dominated), when the take rule erases it
/// (@c Verdict::EraseAndRelease: pruned, or of infinite cost), or when the solve ends.
///
/// @tparam ContainerType The per-node label container, comparing labels in the search's
///                       direction.
template <typename ContainerType>
class KeepAtNodes {
    public:
        static constexpr bool records_at_take = false;  // solutions are read at the end

        /// @brief One empty container per node, each @p prototype's @c copy(), which keeps its
        ///        configuration and direction.
        ///
        /// @param number_of_nodes The number of node positions.
        /// @param prototype An empty container, configured as the search wants: for a backward
        ///                  search, the params' container turned backward.
        void reset(size_t number_of_nodes, const ContainerType& prototype) {
            containers_.clear();
            containers_.reserve(number_of_nodes);
            for (size_t pos = 0; pos < number_of_nodes; ++pos) {
                containers_.emplace_back(prototype.copy());
            }
        }

        /// @brief Forgets every container, releasing no label (the pool owns them).
        void clear() { containers_.clear(); }

        /// @brief The view of the container at node position @p pos.
        NodeView<ContainerType> node(size_t pos) {
            return NodeView<ContainerType>(&containers_.at(pos));
        }

        /// @brief Stores @p label at node position @p pos.
        ///
        /// @return Its position in the node's container.
        template <typename ResourceType>
        typename std::list<Label<ResourceType>*>::iterator store(size_t pos,
                                                                 Label<ResourceType>* label) {
            return containers_.at(pos).add_label(label);
        }

        /// @brief Takes a stored label out of its node's container.
        template <typename IteratorType>
        void erase(const IteratorType& position) {
            containers_.at((*position)->get_end_node()->pos()).erase_label(position);
        }

        /// @brief The labels stored at node position @p pos.
        [[nodiscard]] const auto& labels_at(size_t pos) const {
            return containers_.at(pos).get_labels();
        }

        /// @brief The per-node containers, by node position.
        [[nodiscard]] std::vector<ContainerType>& containers() { return containers_; }

        /// @brief The per-node containers, by node position.
        [[nodiscard]] const std::vector<ContainerType>& containers() const { return containers_; }

    private:
        std::vector<ContainerType> containers_;
};

}  // namespace rcspp::parts
