// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Column generation from scratch: the routes are found by a graph pricer, the master is a model
// the user built in Gurobi, and the loop starts from artificial columns. Skipped without a Gurobi
// license.

#include <gtest/gtest.h>
#include <gurobi_c++.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gurobi_environment.hpp"
#include "rcspp/column_generation/column_generation.hpp"
#include "rcspp/column_generation/graph_pricer.hpp"
#include "rcspp/column_generation/master_problem.hpp"
#include "rcspp_backends/gurobi/gurobi_master_solver.hpp"
#include "test_column_generation_graphs.hpp"

using namespace rcspp;                              // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation;           // NOLINT(google-build-using-namespace)
using namespace rcspp::column_generation::testing;  // NOLINT(google-build-using-namespace)

namespace {

constexpr double tolerance = 1e-6;

// The toy: three customers of demands 4, 5 and 6, vehicles of capacity 11, so a route serves one
// or two of them. From the depot to any customer is 10; customers 1 and 2 are 4 apart, 1 and 3
// are 5, 2 and 3 are 6. Its LP optimum is 37.5 (half of each two-customer route).
constexpr double toy_capacity = 11.0;
const std::vector<double> toy_demands{4.0, 5.0, 6.0};
const std::vector<std::vector<double>> toy_distances{
    {0.0, 10.0, 10.0, 10.0}, {10.0, 0.0, 4.0, 5.0}, {10.0, 4.0, 0.0, 6.0}, {10.0, 5.0, 6.0, 0.0}};
constexpr double toy_lp = 37.5;

// Four customers whose LP optimum, 40, needs routes that visit each customer at most once: with
// capacity 10 and demands of 3, a route could serve a customer twice, and the LP drop below 40.
constexpr double four_capacity = 10.0;
const std::vector<double> four_demands{3.0, 3.0, 5.0, 2.0};
const std::vector<std::vector<double>> four_distances{{0.0, 8.0, 5.0, 6.0, 7.0},
                                                      {8.0, 0.0, 4.0, 10.0, 15.0},
                                                      {5.0, 4.0, 0.0, 6.0, 12.0},
                                                      {6.0, 10.0, 6.0, 0.0, 10.0},
                                                      {7.0, 15.0, 12.0, 10.0, 0.0}};
constexpr double four_lp = 40.0;

// A model per test, in which the user builds the master's rows.
class GurobiColumnGenerationTest : public ::testing::Test {
    protected:
        void SetUp() override {
            GRBEnv* env = gurobi_environment();
            if (env == nullptr) {
                GTEST_SKIP() << "Gurobi has no license on this machine";
            }
            model = std::make_unique<GRBModel>(*env);
        }

        // Column generation over @p graph, from no route at all. The user's rows: "customer i
        // is visited once" for each customer and, with @p vehicles, "at most that many routes",
        // which the master fills as the pricer's convexity row.
        ColumnGenerationResult from_scratch(RoutingGraph& graph, size_t customers,
                                            std::optional<double> vehicles = std::nullopt) {
            std::vector<GRBConstr> rows;
            for (size_t customer = 1; customer <= customers; ++customer) {
                rows.push_back(model->addConstr(GRBLinExpr(),
                                                GRB_EQUAL,
                                                1.0,
                                                "customer " + std::to_string(customer)));
            }
            ConvexityRow fleet;
            if (vehicles.has_value()) {
                rows.push_back(model->addConstr(GRBLinExpr(), GRB_LESS_EQUAL, *vehicles, "fleet"));
                fleet.existing_row = customers;
            }

            GurobiMasterSolver solver(*model, std::move(rows));
            MasterProblem master(solver);
            GraphPricer<RealResource, SizeTBitsetResource> pricer(graph);
            master.add_pricer(pricer, fleet);

            ColumnGeneration loop(master);
            return loop.run();
        }

        std::unique_ptr<GRBModel> model;
};

}  // namespace

TEST_F(GurobiColumnGenerationTest, TheToyFromScratch) {
    auto graph = routing_graph(toy_capacity, toy_demands, toy_distances);

    const ColumnGenerationResult result = from_scratch(*graph, toy_demands.size());

    EXPECT_EQ(result.status, ColumnGenerationStatus::Optimal);  // no artificial column used
    EXPECT_NEAR(result.objective, toy_lp, tolerance);
    EXPECT_NEAR(result.lower_bound, toy_lp, tolerance);
    EXPECT_GT(result.statistics.generated_columns, 0U);
}

TEST_F(GurobiColumnGenerationTest, TheToyWithAFleetLimit) {
    // Two vehicles are enough: the LP optimum uses half of three routes, 1.5 vehicles.
    auto graph = routing_graph(toy_capacity, toy_demands, toy_distances);
    constexpr double two_vehicles = 2.0;
    const ColumnGenerationResult two = from_scratch(*graph, toy_demands.size(), two_vehicles);
    EXPECT_EQ(two.status, ColumnGenerationStatus::Optimal);
    EXPECT_NEAR(two.objective, toy_lp, tolerance);

    // One vehicle is not: the three customers weigh 15, more than its capacity.
    model = std::make_unique<GRBModel>(*gurobi_environment());
    const ColumnGenerationResult one = from_scratch(*graph, toy_demands.size(), 1.0);
    EXPECT_EQ(one.status, ColumnGenerationStatus::Infeasible);
}

TEST_F(GurobiColumnGenerationTest, RoutesThatVisitEachCustomerOnce) {
    auto graph = routing_graph(four_capacity, four_demands, four_distances);

    const ColumnGenerationResult result = from_scratch(*graph, four_demands.size());

    EXPECT_EQ(result.status, ColumnGenerationStatus::Optimal);
    EXPECT_NEAR(result.objective, four_lp, tolerance);
}
