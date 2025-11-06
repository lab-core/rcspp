// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/utils/timer.hpp"

#include <cstdio>
#include <string>

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
  return std::chrono::duration_cast<std::chrono::duration<double>>(elapsed(only_current)).count();
}
int64_t Timer::elapsed_milliseconds(bool only_current) const noexcept {
  return elapsed<std::chrono::milliseconds>(only_current).count();
}
int64_t Timer::elapsed_microseconds(bool only_current) const noexcept {
  return elapsed<std::chrono::microseconds>(only_current).count();
}

constexpr int HOURS_IN_SECONDS = 3600;
constexpr int MINUTES_IN_SECONDS = 60;
constexpr int MAX_LENGTH_HMS = 9;  // "HH:MM:SS" + 1

std::string Timer::elapsed_to_hms(bool only_current) const noexcept {
  const int sec = static_cast<int>(round(elapsed_seconds(only_current)));
  const int h = sec / HOURS_IN_SECONDS;
  const int m = (sec % HOURS_IN_SECONDS) / MINUTES_IN_SECONDS;
  const int ss = sec % MINUTES_IN_SECONDS;
  std::array<char, MAX_LENGTH_HMS> buf;
  std::snprintf(buf.data(), buf.size(), "%02d:%02d:%02d", h, m, ss);
  return {buf.data()};
}

// accumulate another Timer into this (ignores non-finite elapsed values)
Timer& Timer::operator+=(const Timer& other) noexcept {
  // add accumulated of this other timer
  accumulated_ += other.elapsed(false);
  return *this;
}
}  // namespace rcspp
