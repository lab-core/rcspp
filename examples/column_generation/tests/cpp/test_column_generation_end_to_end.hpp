// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Column generation end to end, written once for every master solver: the master is a model the
// user built in their LP solver, the values checked are the same for every solver.
//
// Each solver's test executable gives the tests a user model type and instantiates them:
//
//   struct UserModel {
//       static bool available();                   // false skips the tests (no license)
//       void add_row(double lower, double upper,   // a row the user builds, empty: the
//                    const std::string& name);     // columns fill it (=, >= or <=)
//       MasterSolver& solver();                    // the master solver over the rows added
//   };
//   INSTANTIATE_TYPED_TEST_SUITE_P(Solver, ColumnGenerationEndToEnd, UserModel);
//
// in the namespace rcspp::column_generation::testing, where the tests are.

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/column_generation/column_generation.hpp"
#include "rcspp/column_generation/graph_pricer.hpp"
#include "rcspp/column_generation/master_problem.hpp"
#include "test_column_generation_fakes.hpp"
#include "test_column_generation_graphs.hpp"

namespace rcspp::column_generation::testing {

/// @brief Accuracy of the values checked: within what LP solvers return.
inline constexpr double end_to_end_tolerance = 1e-6;

/// @brief The toy: three customers of demands 4, 5 and 6, each visited exactly once by vehicles of
///        capacity 11, so a route serves one or two of them: the routes {1}, {2}, {3}, {1, 2},
///        {1, 3}, {2, 3}. From the depot to any customer is 10; customers 1 and 2 are 4 apart, 1
///        and 3 are 5, 2 and 3 are 6. The LP optimum 37.5 takes half of each two-customer route;
///        its duals (11.5, 12.5, 13.5) make the three of them tight, and are unique since the
///        optimum is not degenerate. The best plan serves {1, 2} and {3}, at 24 + 20 = 44.
struct Toy {
        static constexpr size_t customers = 3;
        static constexpr double capacity = 11.0;
        static constexpr std::array<double, 6> route_costs{20.0, 20.0, 20.0, 24.0, 25.0, 26.0};
        static constexpr double lp = 37.5;
        static constexpr std::array<double, 3> duals{11.5, 12.5, 13.5};
        static constexpr std::array<double, 6> lp_values{0.0, 0.0, 0.0, 0.5, 0.5, 0.5};
        static constexpr double best_plan = 44.0;
        static constexpr std::array<double, 6> best_plan_values{0.0, 0.0, 1.0, 1.0, 0.0, 0.0};

        static const std::vector<double>& demands() {
            static const std::vector<double> values{4.0, 5.0, 6.0};
            return values;
        }
        static const std::vector<std::vector<double>>& distances() {
            static const std::vector<std::vector<double>> values{{0.0, 10.0, 10.0, 10.0},
                                                                 {10.0, 0.0, 4.0, 5.0},
                                                                 {10.0, 4.0, 0.0, 6.0},
                                                                 {10.0, 5.0, 6.0, 0.0}};
            return values;
        }

        /// @brief Every route, in the order above; they get the ids 0 to 5.
        static std::vector<Solution> routes() {
            const std::vector<std::vector<size_t>> served{{0}, {1}, {2}, {0, 1}, {0, 2}, {1, 2}};
            std::vector<Solution> paths;
            for (size_t route = 0; route < served.size(); ++route) {
                std::vector<Row> rows;
                for (const size_t customer : served[route]) {
                    rows.push_back(Row{.index = customer, .coefficient = 1.0});
                }
                paths.push_back(make_path(route_costs.at(route), std::move(rows)));
            }
            return paths;
        }
};

/// @brief Four customers whose LP optimum, 40, needs routes that visit each customer at most
///        once: with capacity 10 and demands of 3, a route could serve a customer twice, and the
///        LP drop to 37.
struct FourCustomers {
        static constexpr double capacity = 10.0;
        static constexpr double lp = 40.0;

        static const std::vector<double>& demands() {
            static const std::vector<double> values{3.0, 3.0, 5.0, 2.0};
            return values;
        }
        static const std::vector<std::vector<double>>& distances() {
            static const std::vector<std::vector<double>> values{{0.0, 8.0, 5.0, 6.0, 7.0},
                                                                 {8.0, 0.0, 4.0, 10.0, 15.0},
                                                                 {5.0, 4.0, 0.0, 6.0, 12.0},
                                                                 {6.0, 10.0, 6.0, 0.0, 10.0},
                                                                 {7.0, 15.0, 12.0, 10.0, 0.0}};
            return values;
        }
};

/// @brief Whether @p actual matches @p expected, entry by entry, within the tolerance.
inline ::testing::AssertionResult entries_match(std::span<const double> actual,
                                                std::span<const double> expected) {
    if (actual.size() != expected.size()) {
        return ::testing::AssertionFailure()
               << actual.size() << " entries instead of " << expected.size();
    }
    for (size_t entry = 0; entry < actual.size(); ++entry) {
        if (std::abs(actual[entry] - expected[entry]) > end_to_end_tolerance) {
            return ::testing::AssertionFailure() << "entry " << entry << " is " << actual[entry]
                                                 << " instead of " << expected[entry];
        }
    }
    return ::testing::AssertionSuccess();
}

/// @brief The user's rows "customer i is visited once", one per customer.
template <typename UserModel>
void add_customer_rows(UserModel& model, size_t customers) {
    for (size_t customer = 1; customer <= customers; ++customer) {
        model.add_row(1.0, 1.0, "customer " + std::to_string(customer));
    }
}

/// @brief The value of the columns 0 to @p count − 1.
inline std::vector<double> column_values(const MasterSolver& solver, size_t count) {
    std::vector<ColumnId> ids(count);
    std::iota(ids.begin(), ids.end(), ColumnId{0});
    std::vector<double> values(count);
    solver.primal(ids, values);
    return values;
}

/// @brief Column generation over @p graph, from no route at all: the user's customer rows and,
///        with @p vehicles, their row "at most that many routes", which the master fills as the
///        pricer's convexity row.
template <typename UserModel>
ColumnGenerationResult from_scratch(UserModel& model, RoutingGraph& graph, size_t customers,
                                    std::optional<double> vehicles = std::nullopt) {
    add_customer_rows(model, customers);
    ConvexityRow fleet;
    if (vehicles.has_value()) {
        model.add_row(-std::numeric_limits<double>::infinity(), *vehicles, "fleet");
        fleet.existing_row = customers;
    }

    MasterProblem master(model.solver());
    GraphPricer<RealResource, SizeTBitsetResource> pricer(graph);
    master.add_pricer(pricer, fleet);

    ColumnGeneration loop(master);
    return loop.run();
}

template <typename UserModel>
class ColumnGenerationEndToEnd : public ::testing::Test {
    protected:
        void SetUp() override {
            if (!UserModel::available()) {
                GTEST_SKIP() << "this solver has no license on this machine";
            }
        }
};

TYPED_TEST_SUITE_P(ColumnGenerationEndToEnd);

// Every route is a starting column, so the pricer finds nothing and proves it.
TYPED_TEST_P(ColumnGenerationEndToEnd, TheToyLpAndItsDuals) {
    TypeParam model;
    add_customer_rows(model, Toy::customers);
    MasterProblem master(model.solver());
    ScriptedPricer pricer;
    master.add_columns(master.add_pricer(pricer), Toy::routes());

    ColumnGeneration loop(master);
    const ColumnGenerationResult result = loop.run();

    EXPECT_EQ(result.status, ColumnGenerationStatus::Optimal);
    EXPECT_NEAR(result.objective, Toy::lp, end_to_end_tolerance);
    ASSERT_EQ(pricer.seen_duals.size(), 1U);
    EXPECT_TRUE(entries_match(pricer.seen_duals[0], Toy::duals));
    EXPECT_TRUE(
        entries_match(column_values(model.solver(), Toy::route_costs.size()), Toy::lp_values));
}

TYPED_TEST_P(ColumnGenerationEndToEnd, TheRestrictedMasterMipFindsTheBestPlan) {
    TypeParam model;
    add_customer_rows(model, Toy::customers);
    MasterProblem master(model.solver());
    ScriptedPricer pricer;
    master.add_columns(master.add_pricer(pricer), Toy::routes());
    ColumnGeneration loop(master);
    static_cast<void>(loop.run());

    ASSERT_EQ(loop.solve_restricted_master_integer({}), SolverStatus::Optimal);

    EXPECT_NEAR(model.solver().objective(), Toy::best_plan, end_to_end_tolerance);
    EXPECT_TRUE(entries_match(column_values(model.solver(), Toy::route_costs.size()),
                              Toy::best_plan_values));
}

TYPED_TEST_P(ColumnGenerationEndToEnd, TheToyFromScratch) {
    auto graph = routing_graph(Toy::capacity, Toy::demands(), Toy::distances());
    TypeParam model;

    const ColumnGenerationResult result = from_scratch(model, *graph, Toy::customers);

    EXPECT_EQ(result.status, ColumnGenerationStatus::Optimal);  // no artificial column used
    EXPECT_NEAR(result.objective, Toy::lp, end_to_end_tolerance);
    EXPECT_NEAR(result.lower_bound, Toy::lp, end_to_end_tolerance);
    EXPECT_GT(result.statistics.generated_columns, 0U);
}

TYPED_TEST_P(ColumnGenerationEndToEnd, TheToyWithAFleetLimit) {
    auto graph = routing_graph(Toy::capacity, Toy::demands(), Toy::distances());

    // Two vehicles are enough: the LP optimum uses half of three routes, 1.5 vehicles.
    TypeParam two_vehicles;
    constexpr double two = 2.0;
    const ColumnGenerationResult with_two = from_scratch(two_vehicles, *graph, Toy::customers, two);
    EXPECT_EQ(with_two.status, ColumnGenerationStatus::Optimal);
    EXPECT_NEAR(with_two.objective, Toy::lp, end_to_end_tolerance);

    // One vehicle is not: the three customers weigh 15, more than its capacity.
    TypeParam one_vehicle;
    const ColumnGenerationResult with_one = from_scratch(one_vehicle, *graph, Toy::customers, 1.0);
    EXPECT_EQ(with_one.status, ColumnGenerationStatus::Infeasible);
}

TYPED_TEST_P(ColumnGenerationEndToEnd, RoutesThatVisitEachCustomerOnce) {
    auto graph = routing_graph(FourCustomers::capacity,
                               FourCustomers::demands(),
                               FourCustomers::distances());
    TypeParam model;

    const ColumnGenerationResult result =
        from_scratch(model, *graph, FourCustomers::demands().size());

    EXPECT_EQ(result.status, ColumnGenerationStatus::Optimal);
    EXPECT_NEAR(result.objective, FourCustomers::lp, end_to_end_tolerance);
}

REGISTER_TYPED_TEST_SUITE_P(ColumnGenerationEndToEnd, TheToyLpAndItsDuals,
                            TheRestrictedMasterMipFindsTheBestPlan, TheToyFromScratch,
                            TheToyWithAFleetLimit, RoutesThatVisitEachCustomerOnce);

}  // namespace rcspp::column_generation::testing
