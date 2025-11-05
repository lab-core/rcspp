// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <chrono>  // NOLINT(build/c++11)

namespace rcspp {

class Timer {
    public:
        using clock = std::chrono::steady_clock;
        using rep = clock::rep;
        using duration = clock::duration;
        using time_point = clock::time_point;

        explicit Timer(bool start_timer = false) noexcept;

        // Start or resume the timer. If already running, does nothing.
        void start() noexcept;

        // Stop/pause the timer and accumulate elapsed time.
        void stop() noexcept;

        // Reset accumulated time and stop the timer.
        void reset() noexcept;

        // Reset and start immediately.
        void restart() noexcept;

        // Query whether the timer is currently running.
        [[nodiscard]] bool running() const noexcept;

        // Return elapsed time cast to the requested Duration type (default milliseconds).
        template <typename Duration = std::chrono::milliseconds>
        [[nodiscard]] Duration elapsed(bool only_current) const noexcept {
            duration total{};
            if (only_current) {
                total = accumulated_;
            }
            if (running_) {
                total += clock::now() - start_time_;
            }
            return std::chrono::duration_cast<Duration>(total);
        }

        // Convenience helpers
        [[nodiscard]] double elapsed_seconds(bool only_current = false) const noexcept;
        [[nodiscard]] int64_t elapsed_milliseconds(bool only_current = false) const noexcept;
        [[nodiscard]] int64_t elapsed_microseconds(bool only_current = false) const noexcept;

    private:
        bool running_{false};
        time_point start_time_;
        duration accumulated_;
};

}  // namespace rcspp
