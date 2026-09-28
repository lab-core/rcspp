// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The single-solve harness for the experiments on bidirectional labeling as a heuristic.
//
// One call runs one algorithm configuration on one pricing subproblem, times it, validates every
// column it returns (util/column_validator.hpp), and reports one row: what the solve cost, what it
// found, and the diagnostics `SolveResult` carries. Rows go to stdout as `[ ROW ] <csv>` and, when
// the environment variable `RCSPP_SWEEP_OUT` names a file, are appended there too, so a long sweep
// can run unattended and be tabulated afterwards.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "heuristics/pricing_model.hpp"
#include "rcspp/rcspp.hpp"
#include "util/column_validator.hpp"

namespace heuristics {

using AlgorithmPtr = std::unique_ptr<rcspp::Algorithm<PricingRC, PricingLC>>;
using AlgorithmMaker = std::function<AlgorithmPtr(PricingGraph&)>;

/// @brief The pricing cutoff: only columns with a negative reduced cost are wanted, as in the VRP
///        example's column generation (`-EPSILON`).
constexpr double kPricingUpperBound = -1e-6;

/// @brief A runaway guard on every run, not a measurement.
constexpr double kGuardSeconds = 120.0;
constexpr double kGuardMemoryGiB = 8.0;

/// @brief How one configuration is labelled in the results.
struct RunConfig {
        std::string algorithm;
        size_t quota = rcspp::MAX_INT;
        size_t join_budget = rcspp::MAX_INT;
        size_t max_pairs = rcspp::MAX_INT;
        double timeout_s = std::numeric_limits<double>::infinity();
        bool join_after_stop = true;
        /// @brief Anything else that distinguishes the run (iterations, solution cap, ...).
        std::string extra;
};

/// @brief One run's outcome.
struct RunRow {
        std::string dataset;
        std::string model;
        RunConfig config;
        std::string status;
        double seconds = 0.0;
        size_t extended = 0;
        size_t forward_labels = 0;
        size_t backward_labels = 0;
        size_t dominance_checks = 0;
        size_t join_pairs = 0;
        size_t joined_paths = 0;
        bool join_truncated = false;
        bool bounded = false;
        double h_used = 0.0;
        bool memory_pressure = false;
        size_t columns = 0;
        size_t negative_columns = 0;
        double best = std::numeric_limits<double>::infinity();
        double top10 = 0.0;
        size_t invalid = 0;
        std::string first_issue;
        /// @brief Every timed repetition, in seconds; `seconds` is their median.
        std::vector<double> samples;
};

/// @brief Reads an environment variable without MSVC's deprecation warning.
inline std::string environment(const char* name) {
#ifdef _MSC_VER
    char* value = nullptr;
    size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) {
        return {};
    }
    std::string text(value);
    free(value);  // NOLINT(cppcoreguidelines-no-malloc)
    return text;
#else
    const char* value = std::getenv(name);  // NOLINT(concurrency-mt-unsafe)
    return value == nullptr ? std::string() : std::string(value);
#endif
}

inline std::string csv_header() {
    return "dataset,model,algorithm,quota,join_budget,max_pairs,timeout_s,join_after_stop,extra,"
           "status,seconds,extended,forward_labels,backward_labels,dominance_checks,join_pairs,"
           "joined_paths,join_truncated,bounded,h_used,memory_pressure,columns,negative_columns,"
           "best,top10,invalid,first_issue";
}

inline std::string limit_text(size_t value) {
    return value >= rcspp::MAX_INT ? std::string("inf") : std::to_string(value);
}

inline std::string to_csv(const RunRow& row) {
    std::ostringstream out;
    out.precision(12);
    out << row.dataset << ',' << row.model << ',' << row.config.algorithm << ','
        << limit_text(row.config.quota) << ',' << limit_text(row.config.join_budget) << ','
        << limit_text(row.config.max_pairs) << ',';
    if (std::isfinite(row.config.timeout_s)) {
        out << row.config.timeout_s;
    } else {
        out << "inf";
    }
    out << ',' << (row.config.join_after_stop ? 1 : 0) << ',' << row.config.extra << ','
        << row.status << ',' << row.seconds << ',' << row.extended << ',' << row.forward_labels
        << ',' << row.backward_labels << ',' << row.dominance_checks << ',' << row.join_pairs << ','
        << row.joined_paths << ',' << (row.join_truncated ? 1 : 0) << ',' << (row.bounded ? 1 : 0)
        << ',' << row.h_used << ',' << (row.memory_pressure ? 1 : 0) << ',' << row.columns << ','
        << row.negative_columns << ',';
    if (std::isfinite(row.best)) {
        out << row.best;
    }
    out << ',' << row.top10 << ',' << row.invalid << ',';
    // The issue text may hold commas; keep the row parseable.
    std::string issue = row.first_issue;
    std::ranges::replace(issue, ',', ';');
    out << issue;
    return out.str();
}

/// @brief Prints a row, and appends it to `RCSPP_SWEEP_OUT` when that is set.
inline void emit(const RunRow& row) {
    const std::string line = to_csv(row);
    std::cout << "[ ROW ] " << line << std::endl;
    if (const std::string path = environment("RCSPP_SWEEP_OUT"); !path.empty()) {
        const bool fresh = !std::ifstream(path).good();
        std::ofstream file(path, std::ios::app);
        if (fresh) {
            file << csv_header() << '\n';
        }
        file << line << '\n';
    }
}

/// @brief Base params every run starts from: the runaway guards, nothing else.
inline rcspp::AlgorithmBaseParams guarded_params() {
    rcspp::AlgorithmBaseParams params;
    params.timeout_s = kGuardSeconds;
    params.max_memory_gb = kGuardMemoryGiB;
    return params;
}

/// @brief Params for a bidirectional run clocked on time, with `H` half the horizon.
inline rcspp::AlgorithmBaseParams bidirectional_params(double horizon) {
    auto params = guarded_params();
    params.critical_resource_index = 1;  // time
    params.half_way_point = horizon / 2.0;
    return params;
}

/// @brief A maker for any two-parameter algorithm template the graph can create.
template <template <typename, typename> class Algo>
AlgorithmMaker make(rcspp::AlgorithmBaseParams params) {
    return [params](PricingGraph& graph) -> AlgorithmPtr {
        return graph.create_algorithm<Algo>(params.with_container(PricingLC()));
    };
}

/// @brief A maker for a bidirectional search on @p params.
///
/// @tparam Strategy Simple's search takes labels in arrival order; Pushing's sweeps the nodes, so
///                  a per-node quota keeps each node's cheapest labels.
template <template <typename, typename> class Strategy = rcspp::SimpleDominanceAlgorithm>
AlgorithmMaker make_bidirectional(rcspp::AlgorithmBaseParams params) {
    params.direction = rcspp::SearchDirection::Bidirectional;
    return make<Strategy>(params);
}

/// @brief A maker for `DiversificationSearch` around an inner algorithm.
inline AlgorithmMaker make_diversification(rcspp::AlgorithmBaseParams params,
                                           AlgorithmMaker inner) {
    return [params, inner = std::move(inner)](PricingGraph& graph) -> AlgorithmPtr {
        return std::make_unique<rcspp::DiversificationSearch<PricingRC, PricingLC>>(
            &graph.get_resource_factory(),
            params.with_container(PricingLC()),
            inner ? inner(graph) : nullptr);
    };
}

/// @brief What a result's columns are worth to a pricing loop.
inline void score(const std::vector<rcspp::Solution>& solutions, RunRow* row) {
    std::vector<double> costs;
    costs.reserve(solutions.size());
    for (const auto& solution : solutions) {
        costs.push_back(solution.cost);
    }
    std::ranges::sort(costs);
    row->columns = costs.size();
    row->negative_columns = static_cast<size_t>(
        std::ranges::count_if(costs, [](double cost) { return cost < kPricingUpperBound; }));
    row->best = costs.empty() ? std::numeric_limits<double>::infinity() : costs.front();
    row->top10 = 0.0;
    for (size_t i = 0; i < costs.size() && i < 10; ++i) {
        row->top10 += std::min(costs[i], 0.0);
    }
}

/// @brief Runs one configuration and returns its row (not yet emitted).
///
/// @param model       The subproblem; its graph is solved in place and restored.
/// @param dataset     A name for the instance and dual vector.
/// @param config      How the run is labelled.
/// @param maker       Builds a fresh algorithm on the graph.
/// @param repetitions Timed repetitions; the median time is reported, the last result scored.
/// @return The row.
inline RunRow run(PricingModel& model, const std::string& dataset, RunConfig config,
                  const AlgorithmMaker& maker, size_t repetitions = 1) {
    RunRow row;
    row.dataset = dataset;
    row.model = model.options().tag();
    row.config = std::move(config);

    std::vector<double> times;
    rcspp::SolveResult result;
    AlgorithmPtr algorithm;
    for (size_t rep = 0; rep < std::max<size_t>(repetitions, 1); ++rep) {
        algorithm = maker(model.graph());
        const auto started = std::chrono::steady_clock::now();
        result = model.graph().solve(algorithm.get(), kPricingUpperBound);
        times.push_back(
            std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());
    }
    std::ranges::sort(times);
    row.seconds = times[times.size() / 2];
    row.samples = std::move(times);

    row.status = result.status_string();
    row.extended = algorithm->get_number_of_extended_labels();
    row.forward_labels = result.forward_labels;
    row.backward_labels = result.backward_labels;
    row.dominance_checks = result.dominance_checks;
    row.join_pairs = result.join_pairs_tested;
    row.joined_paths = result.number_of_joined_paths;
    row.join_truncated = result.join_truncated;
    row.bounded = result.bounded_by_half_way;
    row.h_used = result.half_way_point_used;
    row.memory_pressure = result.memory_pressure_triggered;
    score(result.solutions, &row);

    const auto issues = test_util::validate_columns(model.graph(), result.solutions);
    row.invalid = issues.size();
    if (!issues.empty()) {
        row.first_issue = issues.front().what;
    }
    return row;
}

/// @brief Runs, emits and returns one configuration.
inline RunRow run_and_emit(PricingModel& model, const std::string& dataset, RunConfig config,
                           const AlgorithmMaker& maker, size_t repetitions = 1) {
    RunRow row = run(model, dataset, std::move(config), maker, repetitions);
    emit(row);
    return row;
}

}  // namespace heuristics
