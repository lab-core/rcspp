// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Which overload of Composition::apply a three-argument call reaches. A mutable composition takes
// a second composition or a raw tuple of component vectors; a const one takes only a second
// composition, so a tuple is refused at the call instead of failing inside the library.

#include <gtest/gtest.h>

#include <type_traits>
#include <utility>

#include "rcspp/rcspp.hpp"

namespace composition_apply_test {

using Composed = rcspp::Resource<rcspp::ResourceTypeComposition<rcspp::RealResource>>;
using Components = std::remove_cvref_t<decltype(std::declval<const Composed&>().get_components())>;

inline constexpr auto ternary = [](const auto& /*lhs*/, const auto& /*rhs*/,
                                   const auto& /*second*/) {};

/// @brief Whether `lhs.apply(rhs, second, func)` names an overload: only the declaration is
///        checked, not the body.
template <typename Lhs, typename Second>
concept TernaryApplicable = requires(Lhs& lhs, const Composed& rhs, const Second& second) {
    lhs.apply(rhs, second, ternary);
};

}  // namespace composition_apply_test

TEST(CompositionApply, ConstOverloadTakesOnlyASecondComposition) {
    namespace cat = composition_apply_test;
    static_assert(cat::TernaryApplicable<cat::Composed, cat::Composed>);
    static_assert(cat::TernaryApplicable<cat::Composed, cat::Components>);
    static_assert(cat::TernaryApplicable<const cat::Composed, cat::Composed>);
    static_assert(!cat::TernaryApplicable<const cat::Composed, cat::Components>);
}
