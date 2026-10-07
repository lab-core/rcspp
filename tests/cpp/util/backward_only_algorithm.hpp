// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <limits>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

namespace test_util {

/// @brief A backward-only labeling algorithm, for testing the backward machinery in isolation.
///
/// The backward search of `SimpleDominanceAlgorithm` (`rcspp::detail::BackwardSimple`): it seeds
/// at the sinks and runs backward to completion, with no half-way bound and no join. Its
/// constructor sets `direction = Backward`, and it adds accessors to its labels.
///
/// @tparam ResourceType       The resource type carried by labels.
/// @tparam LabelContainerType The per-node container; defaults to a *backward* label list, since a
///                            forward-comparing container would prune the better labels.
template <typename ResourceType,
          typename LabelContainerType = rcspp::LabelList<ResourceType, rcspp::BackwardDirection>>
    requires rcspp::ResourceTypeConcept<ResourceType>
class BackwardOnlyAlgorithm
    : public rcspp::detail::BackwardSimple<ResourceType, LabelContainerType> {
        using Base = rcspp::detail::BackwardSimple<ResourceType, LabelContainerType>;

    public:
        BackwardOnlyAlgorithm(rcspp::ResourceFactory<ResourceType>* resource_factory,
                              rcspp::AlgorithmParams<LabelContainerType> params)
            : Base(resource_factory, backward(std::move(params))) {}

        ~BackwardOnlyAlgorithm() override = default;

        /// @brief Read-only access to the per-node backward label sets, for assertions on values.
        [[nodiscard]] const std::vector<typename Base::DirectedContainer>& get_labels_by_node_pos()
            const {
            return this->non_dominated_labels_by_node_pos_;
        }

        /// @brief The labels at this search's terminal nodes, i.e. the graph's *sources*
        ///        (the inherited `get_labels_at_sinks()` follows the search direction).
        [[nodiscard]] std::list<rcspp::Label<ResourceType>*> get_terminal_labels() const {
            return this->get_labels_at_sinks();
        }

        /// @brief The cheapest complete backward path, read from the terminal labels.
        ///
        /// @return The minimum cost over terminal labels, or infinity when there are none.
        [[nodiscard]] double best_terminal_cost() const {
            double best = std::numeric_limits<double>::infinity();
            for (const auto* label : get_terminal_labels()) {
                best = std::min(best, label->get_cost());
            }
            return best;
        }

    private:
        static rcspp::AlgorithmParams<LabelContainerType> backward(
            rcspp::AlgorithmParams<LabelContainerType> params) {
            params.direction = rcspp::SearchDirection::Backward;
            return params;
        }
};

/// @brief Builds a `BackwardOnlyAlgorithm` on @p graph's resources.
///
/// `ResourceGraph::create_algorithm` returns an `Algorithm` pointer, which hides the accessors.
///
/// @param graph  The graph whose resource factory the algorithm uses.
/// @param params The algorithm's params; the direction is set to backward.
/// @return The algorithm, with its concrete type.
template <typename LabelContainerType, typename... ResourceTypes>
[[nodiscard]] auto make_backward_only(rcspp::ResourceGraph<ResourceTypes...>* graph,
                                      rcspp::AlgorithmParams<LabelContainerType> params) {
    using ResourceType = rcspp::ResourceTypeComposition<ResourceTypes...>;
    return std::make_unique<BackwardOnlyAlgorithm<ResourceType, LabelContainerType>>(
        &graph->get_resource_factory(),
        std::move(params));
}

}  // namespace test_util
