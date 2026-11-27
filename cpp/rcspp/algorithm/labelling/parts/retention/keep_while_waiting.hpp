// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
#include <list>
#include <type_traits>
#include <utility>
#include <vector>

#include "rcspp/algorithm/labelling/parts/admission/node_view.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp::parts {

/// @brief A retention that stores no label at the nodes: a label lives only while it waits in
///        the schedule, or while a label extended from it waits. The heuristics use it.
///
/// Since nothing is stored at the end nodes, no path can be read from there when the run ends.
/// Instead, a path is recorded as soon as its end-node label is taken (@ref records_at_take).
/// Once the step is done with a label it took (it extended it, or recorded it), it releases it
/// (@ref releases_taken_labels). The release goes through the reference counts: a label stays
/// alive as long as some label extended from it, directly or not, is still waiting. So the path
/// of every waiting label stays whole.
///
/// With nothing stored, no label at a node can dominate a new one: the admission rule sees an
/// @ref EmptyNodeView, so any rule keeps every feasible label, as @ref AdmitAll does.
///
/// @tparam ContainerType The per-node label container of the params, which it does not use.
template <typename ContainerType>
class KeepWhileWaiting {
        using labels_type =
            std::remove_cvref_t<decltype(std::declval<const ContainerType&>().get_labels())>;

    public:
        /// @brief A solution is recorded when its end-node label is taken: nothing is left at the
        ///        end nodes to read when the loop ends.
        static constexpr bool records_at_take = true;
        /// @brief The step releases every label it takes once it is done with it: nothing is
        ///        kept behind (@ref releases_taken_labels_v).
        static constexpr bool releases_taken_labels = true;

        /// @brief Does nothing: no container is kept, whatever the number of nodes.
        void reset(size_t /*number_of_nodes*/, const ContainerType& /*prototype*/) {}

        /// @brief Does nothing: no label is stored, so none is forgotten.
        void clear() {}

        /// @brief The view of a node: nothing is stored there.
        EmptyNodeView node(size_t /*pos*/) { return {}; }

        /// @brief Stores nothing: the label lives in the schedule while it waits.
        ///
        /// @return A position that no container holds; @ref erase ignores it.
        template <typename ResourceType>
        typename std::list<Label<ResourceType>*>::iterator store(size_t /*pos*/,
                                                                 Label<ResourceType>* /*label*/) {
            return {};
        }

        /// @brief Erases nothing: no label is stored.
        template <typename IteratorType>
        void erase(const IteratorType& /*position*/) {}

        /// @brief The labels stored at node position @p pos: none.
        [[nodiscard]] const labels_type& labels_at(size_t /*pos*/) const { return none_; }

        /// @brief The per-node containers: none.
        [[nodiscard]] const std::vector<ContainerType>& containers() const {
            return no_containers_;
        }

    private:
        labels_type none_;
        std::vector<ContainerType> no_containers_;
};

}  // namespace rcspp::parts
