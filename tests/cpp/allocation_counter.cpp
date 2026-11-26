// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The test executable's global operator new and delete: std::malloc and std::free, counting the
// allocations of a thread while an allocation_counting::AllocationCounter lives on it. The aligned
// and nothrow forms keep their defaults (the nothrow ones call these).

#include "allocation_counter.hpp"

#include <cstddef>
#include <cstdlib>
#include <new>

namespace {

thread_local bool counting = false;
thread_local size_t allocations = 0;

}  // namespace

namespace allocation_counting {

AllocationCounter::AllocationCounter() {
    allocations = 0;
    counting = true;
}

AllocationCounter::~AllocationCounter() {
    counting = false;
}

size_t AllocationCounter::count() const {
    return allocations;
}

}  // namespace allocation_counting

void* operator new(std::size_t size) {
    if (counting) {
        ++allocations;
    }
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {  // NOLINT(*-no-malloc)
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    return operator new(size);
}

void operator delete(void* memory) noexcept {
    std::free(memory);  // NOLINT(*-no-malloc)
}

void operator delete[](void* memory) noexcept {
    std::free(memory);  // NOLINT(*-no-malloc)
}

void operator delete(void* memory, std::size_t /*size*/) noexcept {
    std::free(memory);  // NOLINT(*-no-malloc)
}

void operator delete[](void* memory, std::size_t /*size*/) noexcept {
    std::free(memory);  // NOLINT(*-no-malloc)
}
