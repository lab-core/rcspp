// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/dominance_algorithm.hpp"
#include "rcspp/algorithm/labelling/bidirectional_labelling_algorithm.hpp"
#include "rcspp/algorithm/search_direction.hpp"
#include "rcspp/algorithm/simple_dominance_algorithm.hpp"
#include "rcspp/resource/concrete/numerical_resource.hpp"
#include "rcspp/utils/utils.hpp"

namespace rcspp::detail {

/// @brief Which directions a strategy can search in besides forward, and the classes that do it.
///
/// The primary template says: forward only. A strategy that searches in another direction
/// specialises it.
///
/// @tparam Strategy The algorithm template a caller names, e.g. @c SimpleDominanceAlgorithm.
template <template <typename, typename> class Strategy>
struct DirectionalImplementations {
        static constexpr bool has_backward = false;
        static constexpr bool has_bidirectional = false;
};

/// @brief @c SimpleDominanceAlgorithm also searches backward, as
///        @ref BackwardSimpleDominanceAlgorithm, and bidirectionally, as
///        @ref BidirectionalLabellingAlgorithm.
template <>
struct DirectionalImplementations<SimpleDominanceAlgorithm> {
        static constexpr bool has_backward = true;
        template <typename ResourceType, typename LabelContainerType>
        using Backward = BackwardSimpleDominanceAlgorithm<ResourceType, LabelContainerType>;
        static constexpr bool has_bidirectional = true;
        template <typename ResourceType, typename LabelContainerType, typename CriticalResourceType>
        using Bidirectional =
            BidirectionalLabellingAlgorithm<ResourceType, LabelContainerType, CriticalResourceType>;
};

/// @brief Whether constructor arguments @p Args are exactly one
///        @c AlgorithmParams<LabelContainerType>: then @c ResourceGraph::create_algorithm picks the
///        class from the params' direction.
template <typename LabelContainerType, typename... Args>
inline constexpr bool is_params_only_v =
    sizeof...(Args) == 1 &&
    (std::is_same_v<std::remove_cvref_t<Args>, AlgorithmParams<LabelContainerType>> && ...);

/// @brief Builds the class that searches in `params.direction` for @p Strategy.
///
/// @tparam Strategy             The algorithm template the caller names.
/// @tparam ResourceType         The resource type carried by labels.
/// @tparam LabelContainerType   The per-node container of the params.
/// @tparam CostResourceType     The cost resource type; the clock's type defaults to it.
/// @tparam CriticalResourceType The type of the bidirectional search's clock, the critical
///                              resource.
/// @param resource_factory The graph's resource factory.
/// @param params           The algorithm's params; their direction picks the class.
/// @return The algorithm, behind its base class.
/// @throws std::invalid_argument when @p Strategy cannot search in that direction, or a
///         backward or bidirectional search is asked for with a container that has no backward
///         form.
template <template <typename, typename> class Strategy, typename ResourceType,
          typename LabelContainerType, typename CostResourceType = RealResource,
          typename CriticalResourceType = CostResourceType>
std::unique_ptr<Algorithm<ResourceType, LabelContainerType>> make_directional_algorithm(
    ResourceFactory<ResourceType>* resource_factory, AlgorithmParams<LabelContainerType> params) {
    using Impl = DirectionalImplementations<Strategy>;
    const SearchDirection direction = params.direction;
    switch (direction) {
        case SearchDirection::Forward:
            return std::make_unique<Strategy<ResourceType, LabelContainerType>>(resource_factory,
                                                                                std::move(params));
        case SearchDirection::Backward:
            if constexpr (Impl::has_backward) {
                if constexpr (has_directed_container_v<LabelContainerType, BackwardDirection>) {
                    return std::make_unique<
                        typename Impl::template Backward<ResourceType, LabelContainerType>>(
                        resource_factory,
                        std::move(params));
                } else {
                    throw std::invalid_argument(
                        template_name<LabelContainerType>() +
                        " cannot compare backward: a backward search keeps its labels in the "
                        "container its params hold, turned backward, so the container must "
                        "declare its own with_direction<TargetDirection>()");
                }
            }
            break;
        case SearchDirection::Bidirectional:
            if constexpr (Impl::has_bidirectional) {
                // The backward half keeps its labels in the same container, turned backward.
                if constexpr (has_directed_container_v<LabelContainerType, BackwardDirection>) {
                    return std::make_unique<
                        typename Impl::template Bidirectional<ResourceType,
                                                              LabelContainerType,
                                                              CriticalResourceType>>(
                        resource_factory,
                        std::move(params));
                } else {
                    throw std::invalid_argument(
                        template_name<LabelContainerType>() +
                        " cannot compare backward: a bidirectional search keeps its backward "
                        "labels "
                        "in the container its params hold, turned backward, so the container "
                        "must declare its own with_direction<TargetDirection>()");
                }
            }
            break;
    }
    throw std::invalid_argument(template_name<Strategy<ResourceType, LabelContainerType>>() +
                                " does not support the " + std::string(to_string(direction)) +
                                " direction (see Algorithm::supported_directions())");
}

}  // namespace rcspp::detail
