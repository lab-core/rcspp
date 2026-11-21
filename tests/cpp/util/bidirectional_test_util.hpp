// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <stdexcept>

#include "rcspp/rcspp.hpp"

namespace test_util {

/// @brief @p params with `direction = Bidirectional`, everything else unchanged.
template <typename ParamsType>
[[nodiscard]] ParamsType bidirectional(ParamsType params) {
    params.direction = rcspp::SearchDirection::Bidirectional;
    return params;
}

/// @brief The internal bidirectional search behind @p algorithm, for tests that read its
///        internals.
///
/// @tparam BidirectionalType The internal type, a `rcspp::detail::BidirectionalLabellingAlgorithm`.
/// @param algorithm An algorithm `create_algorithm` built with `direction = Bidirectional`.
/// @return @p algorithm as @p BidirectionalType.
/// @throws std::logic_error when @p algorithm is not a @p BidirectionalType.
template <typename BidirectionalType, typename ResourceType, typename LabelContainerType>
[[nodiscard]] BidirectionalType* bidirectional_impl(
    rcspp::Algorithm<ResourceType, LabelContainerType>* algorithm) {
    auto* impl = dynamic_cast<BidirectionalType*>(algorithm);
    if (impl == nullptr) {
        throw std::logic_error("not a bidirectional search of this type");
    }
    return impl;
}

}  // namespace test_util
