// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <pybind11/pybind11.h>

#include <atomic>
#include <optional>
#include <type_traits>
#include <utility>

namespace py = pybind11;

// ─── SIGINT globals (defined in graph.cpp) ────────────────────────────────────
// g_py_interrupted: set by the C signal handler; polled by the labelling loop.
// g_active_calls:   counts long-running C++ bindings holding the release guard.
extern std::atomic<bool> g_py_interrupted;
extern std::atomic<int> g_active_calls;

// ─── run_interruptible ────────────────────────────────────────────────────────
// Releases the GIL, calls f(), re-acquires the GIL, then raises KeyboardInterrupt
// if g_py_interrupted was set during the call.  Handles both void and non-void callables.
class ActiveCall {
    private:
        struct ActiveCallGuard {
                ActiveCallGuard() { g_active_calls.fetch_add(1, std::memory_order_relaxed); }
                ~ActiveCallGuard() { g_active_calls.fetch_sub(1, std::memory_order_relaxed); }
                ActiveCallGuard(const ActiveCallGuard&) = delete;
                ActiveCallGuard& operator=(const ActiveCallGuard&) = delete;
        };

    public:
        static bool any_active() { return g_active_calls.load(std::memory_order_relaxed) > 0; }

        static bool is_interrupted() { return g_py_interrupted.load(std::memory_order_relaxed); }

        template <typename F>
        static auto run_interruptible(F&& f) {
            using R = std::invoke_result_t<F>;
            if constexpr (std::is_void_v<R>) {
                {
                    ActiveCallGuard guard;
                    py::gil_scoped_release release;
                    std::forward<F>(f)();
                }
                check_if_throw_error();
            } else {
                std::optional<R> result;
                {
                    ActiveCallGuard guard;
                    py::gil_scoped_release release;
                    result.emplace(std::forward<F>(f)());
                }
                check_if_throw_error();
                return std::move(*result);
            }
        }

        static void mark_interrupted() { g_py_interrupted.store(true, std::memory_order_relaxed); }

        static void check_if_throw_error() {
            if (g_py_interrupted.exchange(false, std::memory_order_relaxed)) {
                PyErr_SetNone(PyExc_KeyboardInterrupt);
                throw py::error_already_set();
            }
        }
};
