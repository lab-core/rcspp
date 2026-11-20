// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>

namespace allocation_counting {

/// @brief Counts the heap allocations this thread makes through the global @c operator new while
///        it lives.
///
/// The test executable replaces the global @c operator new and @c operator delete with ones that
/// call @c std::malloc and @c std::free, and count while a counter lives
/// (@c allocation_counter.cpp). Counters do not nest.
class AllocationCounter {
    public:
        AllocationCounter();
        ~AllocationCounter();
        AllocationCounter(const AllocationCounter&) = delete;
        AllocationCounter& operator=(const AllocationCounter&) = delete;
        AllocationCounter(AllocationCounter&&) = delete;
        AllocationCounter& operator=(AllocationCounter&&) = delete;

        /// @brief The allocations counted so far.
        [[nodiscard]] size_t count() const;
};

}  // namespace allocation_counting
