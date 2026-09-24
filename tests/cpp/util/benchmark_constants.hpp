// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Constants shared by the benchmarks. Kept in util/ so the reference optimum has one definition
// that any benchmark TU can include.

namespace bidirectional_benchmark {

constexpr double kTolerance = 1e-9;
constexpr double kOptimal = -319.87786809696524415;

/// @brief A runaway guard, not a measurement: four hours per solve. It must be high enough for
/// the slower forward reference to finish, or the comparison silently becomes a truncated one.
constexpr double kTimeoutSeconds = 14400.0;

/// @brief How attractive a customer is under the synthetic duals; see `synthetic_duals`.
constexpr double kDualAlpha = 1.0;

/// @brief A hard cap on process RSS, so a runaway solve stops rather than taking the machine down.
constexpr double kMemoryCapGiB = 8.0;

/// @brief The equal time budget each algorithm gets on the full-size instances, which do not
/// finish; the test compares how far each search gets.
constexpr double kBudgetSeconds = 15.0;

}  // namespace bidirectional_benchmark
