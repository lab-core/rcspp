// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <stdexcept>

#include "rcspp/rcspp.hpp"

namespace test_util {

/// @brief @p params with `direction = Bidirectional`, everything else unchanged.
template <typename Params>
[[nodiscard]] Params bidirectional(Params params) {
    params.direction = rcspp::SearchDirection::Bidirectional;
    return params;
}

/// @brief The internal bidirectional search behind @p algorithm, for tests that read its
///        internals.
///
/// @tparam Impl The internal type, a `rcspp::detail::BidirectionalDominanceAlgorithm`.
/// @param algorithm An algorithm `create_algorithm` built with `direction = Bidirectional`.
/// @return @p algorithm as @p Impl.
/// @throws std::logic_error when @p algorithm is not an @p Impl.
template <typename Impl, typename R, typename LC>
[[nodiscard]] Impl* bidirectional_impl(rcspp::Algorithm<R, LC>* algorithm) {
    auto* impl = dynamic_cast<Impl*>(algorithm);
    if (impl == nullptr) {
        throw std::logic_error("not a bidirectional search of this type");
    }
    return impl;
}

}  // namespace test_util
