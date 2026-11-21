// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Column generation for the vehicle routing problem, with the C++ column generation example
// (examples/column_generation).
//
// The master chooses routes so that every customer is visited exactly once; pricing finds new
// routes in the graph of vrp/graph.hpp. The master's LP is solved by Gurobi or by HiGHS,
// whichever the build has.

#pragma once

#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "rcspp/column_generation.hpp"
#include "vrp/graph.hpp"
#include "vrp/instance.hpp"

namespace vrp {

/// @brief The master's LP solver.
enum class Master {
    Gurobi,  ///< Gurobi's C++ API; needs USE_COLUMN_GENERATION_EXAMPLE_GUROBI
    Highs,   ///< HiGHS; needs USE_COLUMN_GENERATION_EXAMPLE_HIGHS
};

/// @brief The master @p name gives (`gurobi`, `highs`), or nothing.
[[nodiscard]] std::optional<Master> parse_master(std::string_view name);

/// @brief The names of the masters this build has.
[[nodiscard]] std::vector<std::string_view> built_masters();

/// @brief A route of the integer solution: the customers in the order visited.
struct Route {
        std::vector<size_t> customers;
        double distance = 0.0;
};

/// @brief A master solver over a new model with one row per customer, `= 1`, and no column.
///
/// The model is the LP solver's own, held with the solver: the pointer keeps both alive.
///
/// @throws std::invalid_argument When the build has not @p master.
[[nodiscard]] std::shared_ptr<rcspp::column_generation::MasterSolver> master_solver(
    Master master, size_t customers);

/// @brief One route per customer, depot to customer and back: with them, the first master LP
///        has a solution.
///
/// A starting column is a @c Solution whose @c column holds the route's cost and the rows it
/// covers; its path is only there to be read back.
[[nodiscard]] std::vector<rcspp::Solution> starting_routes(const Instance& instance);

/// @brief The options of @c ColumnGenerationModel.
struct ColumnGenerationOptions {
        /// @brief The most routes a pricing call returns; none returns every improving route.
        std::optional<size_t> max_solutions;
        /// @brief The run's time limit, in seconds.
        double timeout_s = std::numeric_limits<double>::infinity();
        /// @brief Called after each iteration, e.g. to print it.
        std::function<void(const rcspp::column_generation::IterationInfo&)> on_iteration;
};

/// @brief The master over a graph's routes, started from @c starting_routes, and the loop that
///        solves its LP: run it with `loop.run()`.
///
/// Built in place and never moved: the loop holds the master, the master its solver and pricer,
/// the pricer the graph, all by reference. The graph outlives it.
struct ColumnGenerationModel {
        using Pricer =
            rcspp::column_generation::GraphPricer<rcspp::RealResource, rcspp::SizeTBitsetResource>;

        /// @param instance The customers.
        /// @param graph    The pricing graph, from @c pricing_graph.
        /// @param master   The master's LP solver.
        /// @param options  The pricing's cap, the time limit, the iteration hook.
        ColumnGenerationModel(const Instance& instance, Graph& graph, Master master,
                              const ColumnGenerationOptions& options = {});

        ColumnGenerationModel(const ColumnGenerationModel&) = delete;
        ColumnGenerationModel& operator=(const ColumnGenerationModel&) = delete;
        ColumnGenerationModel(ColumnGenerationModel&&) = delete;
        ColumnGenerationModel& operator=(ColumnGenerationModel&&) = delete;
        ~ColumnGenerationModel() = default;

        std::shared_ptr<rcspp::column_generation::MasterSolver> solver;
        rcspp::column_generation::MasterProblem problem;
        Pricer pricer;
        rcspp::column_generation::ColumnGeneration loop;
};

/// @brief The integer solve's time limit when none is given, in seconds.
inline constexpr double default_integer_timeout_s = 60.0;

/// @brief Solves the master with integer columns, over the routes generated so far, and reads
///        its routes (none when it found no solution).
///
/// This is a heuristic: the best plan may need a route that column generation never produced.
[[nodiscard]] std::pair<rcspp::column_generation::SolverStatus, std::vector<Route>> integer_routes(
    ColumnGenerationModel& model, double timeout_s = default_integer_timeout_s);

}  // namespace vrp
