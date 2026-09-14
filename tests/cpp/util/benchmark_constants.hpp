// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The benchmark's shared constants.
//
// Under util/ rather than in the benchmark header itself because `kOptimal` is a reference
// optimum, and a reference optimum wants exactly one definition: any second benchmark that
// measures the same instance has to assert the same number, and two copies of it is precisely the
// drift a single definition avoids. A test_*.hpp may be included by exactly one translation unit,
// by the rule at the top of test_main.cpp, so it could not be shared even if it wanted to be; a
// util/ header carries no TEST macros and may.

namespace bidirectional_benchmark {

constexpr double kTolerance = 1e-9;
constexpr double kOptimal = -319.87786809696524415;

/// @brief A guard, not a measurement.
///
/// Deliberately far above what any instance in the exact-comparison table needs -- R201_50, the
/// slowest, finishes its forward reference in 78 s -- so that this only fires when something has
/// genuinely run away, and never truncates a row that would otherwise have completed. A
/// coverage-instrumented build is several times slower than a plain one, and a limit tight enough
/// to bind there would silently turn an exact comparison into a truncated one.
constexpr double kTimeoutSeconds = 1800.0;

/// @brief How attractive a customer is under the synthetic duals; see `synthetic_duals`.
constexpr double kDualAlpha = 1.0;

/// @brief A hard cap on process RSS, so a runaway solve stops rather than taking the machine down.
constexpr double kMemoryCapGiB = 8.0;

/// @brief The budget each algorithm gets on the full-size instances.
///
/// Those solves do not finish -- see `LargeInstancesUnderATimeBudget` -- so the measurement is what
/// each search reaches within an equal budget rather than what it costs to finish. Fixing the
/// budget also fixes what this test costs, on any build: instrumentation changes how far each
/// search gets, not how long the test takes.
constexpr double kBudgetSeconds = 15.0;

}  // namespace bidirectional_benchmark
