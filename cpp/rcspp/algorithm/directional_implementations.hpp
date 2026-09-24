// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/bidirectional_dominance_algorithm.hpp"
#include "rcspp/algorithm/dominance_algorithm.hpp"
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
        template <typename ResourceType, typename LabelContainerType, typename ClockResourceType>
        using Bidirectional =
            BidirectionalDominanceAlgorithm<ResourceType, LabelContainerType, ClockResourceType>;
};

/// @brief Whether constructor arguments @p Args are exactly one @c AlgorithmParams<LC>: then
///        @c ResourceGraph::create_algorithm picks the class from the params' direction.
template <typename LC, typename... Args>
inline constexpr bool is_params_only_v =
    sizeof...(Args) == 1 && (std::is_same_v<std::remove_cvref_t<Args>, AlgorithmParams<LC>> && ...);

/// @brief Builds the class that searches in `params.direction` for @p Strategy.
///
/// @tparam Strategy           The algorithm template the caller names.
/// @tparam ResourceType       The resource type carried by labels.
/// @tparam LabelContainerType The per-node container of the params.
/// @tparam CostResourceType   The cost resource type; the clock's type defaults to it.
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
                        "a backward search keeps its labels in the container its params hold, "
                        "turned backward, and this container has no backward form: it must declare "
                        "its own with_direction<D>(), as LabelList and LabelBuckets do");
                }
            }
            break;
        case SearchDirection::Bidirectional:
            if constexpr (Impl::has_bidirectional) {
                return std::make_unique<typename Impl::template Bidirectional<ResourceType,
                                                                              LabelContainerType,
                                                                              ClockResourceType>>(
                    resource_factory,
                    std::move(params));
            }
            break;
    }
    throw std::invalid_argument("this algorithm does not support the " +
                                std::string(to_string(direction)) + " direction");
}

}  // namespace rcspp::detail
