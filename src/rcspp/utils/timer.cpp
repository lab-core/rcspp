// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/utils/timer.hpp"

namespace rcspp {
Timer::Timer(bool start_timer) noexcept : accumulated_(duration::zero()) {
    if (start_timer) {
        start();
    }
}

// Start or resume the timer. If already running, does nothing.
void Timer::start() noexcept {
    if (!running_) {
        start_time_ = clock::now();
        running_ = true;
    }
}

// Stop/pause the timer and accumulate elapsed time.
void Timer::stop() noexcept {
    if (running_) {
        accumulated_ += clock::now() - start_time_;
        running_ = false;
    }
}

// Reset accumulated time and stop the timer.
void Timer::reset() noexcept {
    running_ = false;
    accumulated_ = duration::zero();
}

// Reset and start immediately.
void Timer::restart() noexcept {
    accumulated_ = duration::zero();
    start_time_ = clock::now();
    running_ = true;
}

// Query whether the timer is currently running.
bool Timer::running() const noexcept {
    return running_;
}

// Convenience helpers
double Timer::elapsed_seconds(bool only_current) const noexcept {
    // Use the finest common resolution (nanoseconds) then convert to double seconds
    return std::chrono::duration_cast<std::chrono::duration<double>>(
               elapsed<std::chrono::nanoseconds>(only_current))
        .count();
}
int64_t Timer::elapsed_milliseconds(bool only_current) const noexcept {
    return elapsed<std::chrono::milliseconds>(only_current).count();
}
int64_t Timer::elapsed_microseconds(bool only_current) const noexcept {
    return elapsed<std::chrono::microseconds>(only_current).count();
}
}  // namespace rcspp
