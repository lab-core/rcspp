// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <chrono>  // NOLINT(build/c++11)
#include <string>

namespace rcspp {

/// @brief High-resolution stopwatch based on `std::chrono::steady_clock`.
///
/// The timer can be started, stopped, and restarted.  Accumulated time is
/// preserved across start/stop cycles, enabling use as a pausing stopwatch.
/// All public methods are `noexcept`.
class Timer {
    public:
        /// @brief Underlying clock type.
        using clock = std::chrono::steady_clock;
        /// @brief Tick representation type.
        using rep = clock::rep;
        /// @brief Duration type (nanosecond resolution on most platforms).
        using duration = clock::duration;
        /// @brief Time-point type.
        using time_point = clock::time_point;

        /// @brief Constructs a Timer, optionally starting it immediately.
        ///
        /// @param start_timer  When true the timer starts running at construction
        ///                     (default: false).
        explicit Timer(bool start_timer = false) noexcept;

        /// @brief Starts or resumes the timer.
        ///
        /// If the timer is already running, this call is a no-op.
        void start() noexcept;

        /// @brief Stops the timer and adds the current interval to the accumulator.
        ///
        /// If the timer is not running, this call is a no-op.
        void stop() noexcept;

        /// @brief Resets accumulated time to zero and stops the timer.
        void reset() noexcept;

        /// @brief Resets accumulated time and starts the timer immediately.
        void restart() noexcept;

        /// @brief Returns whether the timer is currently running.
        ///
        /// @return True when the timer is running, false when stopped.
        [[nodiscard]] bool running() const noexcept;

        /// @brief Returns the elapsed time cast to the requested duration type.
        ///
        /// When @p only_current is false the returned value includes all
        /// previously accumulated intervals plus any currently-running interval.
        /// When @p only_current is true only the time since the last @ref start()
        /// call is returned (0 if not running).
        ///
        /// @tparam Duration  `std::chrono` duration type to return
        ///                   (default: `std::chrono::nanoseconds`).
        /// @param  only_current  When true, exclude previously accumulated time.
        /// @return Elapsed duration cast to @p Duration.
        template <typename Duration = std::chrono::nanoseconds>
        [[nodiscard]] Duration elapsed(bool only_current) const noexcept {
            duration total{};
            if (!only_current) {
                total = accumulated_;
            }
            if (running_) {
                total += clock::now() - start_time_;
            }
            return std::chrono::duration_cast<Duration>(total);
        }

        /// @brief Returns elapsed time as a floating-point number of seconds.
        ///
        /// @param only_current  When true, exclude previously accumulated time
        ///                      (default: false).
        /// @return Elapsed seconds as a `double`.
        [[nodiscard]] double elapsed_seconds(bool only_current = false) const noexcept;

        /// @brief Returns elapsed time in whole milliseconds.
        ///
        /// @param only_current  When true, exclude previously accumulated time
        ///                      (default: false).
        /// @return Elapsed milliseconds as `int64_t`.
        [[nodiscard]] int64_t elapsed_milliseconds(bool only_current = false) const noexcept;

        /// @brief Returns elapsed time in whole microseconds.
        ///
        /// @param only_current  When true, exclude previously accumulated time
        ///                      (default: false).
        /// @return Elapsed microseconds as `int64_t`.
        [[nodiscard]] int64_t elapsed_microseconds(bool only_current = false) const noexcept;

        /// @brief Returns elapsed time formatted as `HH:MM:SS.ss`.
        ///
        /// @param only_current  When true, exclude previously accumulated time
        ///                      (default: false).
        /// @return Formatted time string.
        [[nodiscard]] std::string elapsed_to_hms(bool only_current = false) const noexcept;

        /// @brief Accumulates the elapsed time of @p other into this timer.
        ///
        /// The full accumulated duration of @p other (including any running
        /// interval) is added to this timer's accumulator.  Non-finite values
        /// are ignored.
        ///
        /// @param other  Timer whose elapsed time to add.
        /// @return Reference to @c *this.
        Timer& operator+=(const Timer& other) noexcept;

    private:
        bool running_{false};
        time_point start_time_;
        duration accumulated_;
};

}  // namespace rcspp
