// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Parity of the bidirectional labelling class with its frozen copy, with label lists and with
// buckets. Its own translation unit: the copies double the labelling templates' instantiations.

#include <gtest/gtest.h>

#include "reference/bidirectional_dominance_algorithm.hpp"
#include "util/parity.hpp"

namespace {

using parity::Buckets;
using parity::Composed;
using parity::List;
using rcspp::RealResource;
using rcspp::SearchDirection;
namespace ref = rcspp::reference;

}  // namespace

TEST(Parity, SimpleBidirectional) {
    parity::sweep<
        parity::Observed<
            ref::detail::BidirectionalDominanceAlgorithm<Composed, List, RealResource>>,
        parity::Observed<
            rcspp::detail::BidirectionalDominanceAlgorithm<Composed, List, RealResource>>>(
        SearchDirection::Bidirectional,
        List{});
    // Both sides keep buckets, the backward side's turned backward.
    parity::sweep<
        parity::Observed<
            ref::detail::BidirectionalDominanceAlgorithm<Composed, Buckets, RealResource>>,
        parity::Observed<
            rcspp::detail::BidirectionalDominanceAlgorithm<Composed, Buckets, RealResource>>>(
        SearchDirection::Bidirectional,
        Buckets(10, 1, 0),
        /*buckets=*/true);
}
