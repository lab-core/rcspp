// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The bidirectional interleave, on two hand-built sides: which side it takes from.

#include <gtest/gtest.h>

#include <cstddef>

#include "rcspp/rcspp.hpp"

namespace interleave_test {

/// @brief What the interleave reads of a side: the labels waiting and the work done.
struct Side {
        struct Schedule {
                size_t waiting = 0;
                [[nodiscard]] size_t size() const { return waiting; }
        };
        Schedule schedule;
        size_t work = 0;
};

/// @brief A side with @p waiting labels waiting and @p work arcs extended.
inline Side side(size_t waiting, size_t work) {
    return {.schedule = {.waiting = waiting}, .work = work};
}

}  // namespace interleave_test

/// @brief The side that has extended fewer arcs goes next; the forward side on a tie.
TEST(LeastWorkFirst, TakesTheSideWithLessWork) {
    namespace it = interleave_test;
    const rcspp::parts::LeastWorkFirst interleave;
    EXPECT_TRUE(interleave.forward_next(it::side(3, 4), it::side(1, 9)));
    EXPECT_FALSE(interleave.forward_next(it::side(3, 9), it::side(1, 4)));
    EXPECT_TRUE(interleave.forward_next(it::side(3, 5), it::side(1, 5)))
        << "a tie goes to the forward side";
}

/// @brief A side with nothing waiting is never taken, whatever the work.
TEST(LeastWorkFirst, SkipsAnEmptySide) {
    namespace it = interleave_test;
    const rcspp::parts::LeastWorkFirst interleave;
    EXPECT_TRUE(interleave.forward_next(it::side(1, 9), it::side(0, 0)));
    EXPECT_FALSE(interleave.forward_next(it::side(0, 0), it::side(1, 9)));
    EXPECT_FALSE(interleave.forward_next(it::side(0, 0), it::side(0, 0)));
}
