// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/bidirectional_dominance_algorithm.hpp"
#include "rcspp/algorithm/directional_dominance_algorithm.hpp"
#include "rcspp/algorithm/search_direction.hpp"
#include "rcspp/algorithm/simple_dominance_algorithm.hpp"
#include "rcspp/resource/concrete/numerical_resource.hpp"

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

/// @brief @c SimpleDominanceAlgorithm also searches backward, as @ref BackwardSimple, and
///        bidirectionally, as @ref BidirectionalDominanceAlgorithm.
template <>
struct DirectionalImplementations<SimpleDominanceAlgorithm> {
        static constexpr bool has_backward = true;
        template <typename ResourceType, typename LabelContainerType>
        using Backward = BackwardSimple<ResourceType, LabelContainerType>;
        static constexpr bool has_bidirectional = true;
        // The class takes the clock before the cost.
        template <typename ResourceType, typename LabelContainerType, typename CostResourceType,
                  typename ClockResourceType>
        using Bidirectional = BidirectionalDominanceAlgorithm<ResourceType, LabelContainerType,
                                                              ClockResourceType, CostResourceType>;
};

/// @brief Builds the class that searches in `params.direction` for @p Strategy.
///
/// @tparam Strategy           The algorithm template the caller names.
/// @tparam ResourceType       The resource type carried by labels.
/// @tparam LabelContainerType The per-node container of the params.
/// @tparam CostResourceType   The cost resource type (read by the bidirectional search).
/// @tparam ClockResourceType  The type of the bidirectional search's clock, the critical resource.
/// @param resource_factory The graph's resource factory.
/// @param params           The algorithm's params; their direction picks the class.
/// @return The algorithm, behind its base class.
/// @throws std::invalid_argument when @p Strategy cannot search in that direction, or a
///         backward search is asked for with a container that has no backward form.
template <template <typename, typename> class Strategy, typename ResourceType,
          typename LabelContainerType, typename CostResourceType = RealResource,
          typename ClockResourceType = CostResourceType>
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
                if constexpr (can_rebind_direction_v<LabelContainerType, BackwardDirection>) {
                    return std::make_unique<
                        typename Impl::template Backward<ResourceType, LabelContainerType>>(
                        resource_factory,
                        std::move(params));
                } else {
                    throw std::invalid_argument(
                        "a backward search needs a LabelList container: this one has no backward "
                        "form");
                }
            }
            break;
        case SearchDirection::Bidirectional:
            if constexpr (Impl::has_bidirectional) {
                return std::make_unique<typename Impl::template Bidirectional<ResourceType,
                                                                              LabelContainerType,
                                                                              CostResourceType,
                                                                              ClockResourceType>>(
                    resource_factory,
                    std::move(params));
            }
            break;
    }
    throw std::invalid_argument("this algorithm does not support the " +
                                std::string(to_string(direction)) +
                                " direction; in this version, only SimpleDominanceAlgorithm "
                                "searches backward or bidirectionally");
}

}  // namespace rcspp::detail
