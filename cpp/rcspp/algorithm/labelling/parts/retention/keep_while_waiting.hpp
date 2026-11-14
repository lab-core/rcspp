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

/// @brief Kept labels live only while they wait to be taken: a heuristic's retention, which keeps
///        nothing behind it.
///
/// Nothing is stored at the nodes. A solution is recorded when its end-node label is taken
/// (`records_at_take`), and the step releases every label it takes once it is done with it
/// (`releases_taken_labels`), through the reference counts: a label lives on while one of the
/// labels extended from it waits, so the path of every waiting label stays whole.
///
/// @tparam ContainerType The per-node label container of the params, which it does not use.
template <typename ContainerType>
class KeepWhileWaiting {
        using Labels =
            std::remove_cvref_t<decltype(std::declval<const ContainerType&>().get_labels())>;

    public:
        static constexpr bool records_at_take = true;        // nothing is left at the end nodes
        static constexpr bool releases_taken_labels = true;  // nothing is kept behind

        void reset(size_t /*number_of_nodes*/, const ContainerType& /*prototype*/) {}

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
        [[nodiscard]] const Labels& labels_at(size_t /*pos*/) const { return none_; }

        /// @brief The per-node containers: none.
        [[nodiscard]] const std::vector<ContainerType>& containers() const {
            return no_containers_;
        }

    private:
        Labels none_;
        std::vector<ContainerType> no_containers_;
};

}  // namespace rcspp::parts
