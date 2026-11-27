// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The C++ VRP example (examples/cpp): its instance reader, its pricing graph and its column
// generation over each master solver built. The command line is tested by ctest (CMakeLists.txt).

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "rcspp/column_generation.hpp"
#include "rcspp/rcspp.hpp"
#include "vrp/column_generation.hpp"
#include "vrp/graph.hpp"
#include "vrp/instance.hpp"

namespace {

namespace cg = rcspp::column_generation;

constexpr double c101_25_lp = 191.8136197786562;

vrp::Customer customer(size_t number, double x, double y, double due_time = 500.0,
                       double demand = 10.0) {
    return vrp::Customer{.number = number,
                         .x = x,
                         .y = y,
                         .demand = demand,
                         .ready_time = 0.0,
                         .due_time = due_time,
                         .service_time = 10.0};
}

vrp::Customer depot() {
    return vrp::Customer{.number = 0, .due_time = 1000.0};
}

// The depot at (0, 0) and three customers around it, all in one vehicle's capacity.
vrp::Instance tiny_instance(double capacity = 50.0) {
    return vrp::Instance{.name = "tiny",
                         .vehicles = 3,
                         .capacity = capacity,
                         .customers = {depot(),
                                       customer(1, 1.0, 0.0),
                                       customer(2, 0.0, 1.0),
                                       customer(3, 1.0, 1.0)}};
}

rcspp::SolveResult solve(vrp::Graph& graph) {
    return graph.solve<rcspp::strategy::Simple>();
}

std::vector<size_t> customers_of(const rcspp::Solution& route, const vrp::Instance& instance) {
    std::vector<size_t> customers;
    for (const size_t node : route.path_node_ids) {
        if (node != 0 && node != vrp::sink(instance)) {
            customers.push_back(node);
        }
    }
    return customers;
}

// Whether Gurobi can build a model on this machine (a licence).
bool gurobi_available() {
    try {
        static_cast<void>(vrp::master_solver(vrp::Master::Gurobi, 1));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<vrp::Master> built() {
    std::vector<vrp::Master> masters;
    for (const std::string_view name : vrp::built_masters()) {
        masters.push_back(*vrp::parse_master(name));
    }
    return masters;
}

std::string name_of(vrp::Master master) {
    return master == vrp::Master::Gurobi ? "gurobi" : "highs";
}

// MARK: Instance

TEST(VrpInstance, ReadSolomon) {
    const vrp::Instance instance = vrp::read_solomon(vrp::find_instance("C101_25"));

    EXPECT_EQ(instance.name, "C101_25");
    EXPECT_EQ(instance.vehicles, 25);
    EXPECT_EQ(instance.capacity, 200.0);
    ASSERT_EQ(instance.customers.size(), 26U);
    EXPECT_EQ(instance.depot(),
              (vrp::Customer{.number = 0, .x = 40.0, .y = 50.0, .due_time = 1236.0}));
    EXPECT_EQ(instance.customers[1],
              (vrp::Customer{.number = 1,
                             .x = 45.0,
                             .y = 68.0,
                             .demand = 10.0,
                             .ready_time = 912.0,
                             .due_time = 967.0,
                             .service_time = 90.0}));
}

TEST(VrpInstance, FindInstanceByNameOrPath) {
    const std::string found = vrp::find_instance("C101_5");

    EXPECT_TRUE(found.ends_with("/C101_5.txt"));
    EXPECT_EQ(vrp::find_instance("C101_5.txt"), found);
    EXPECT_EQ(vrp::find_instance(found), found);
    EXPECT_THROW(static_cast<void>(vrp::find_instance("no_such_instance")), std::runtime_error);
}

TEST(VrpInstance, ReadSolomonRefusesAFileWithoutCustomers) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "rcspp_vrp_test_empty.txt";
    std::ofstream(path) << "EMPTY\n\nVEHICLE\nNUMBER CAPACITY\n 3 50\n";

    EXPECT_THROW(static_cast<void>(vrp::read_solomon(path.string())), std::runtime_error);
    std::filesystem::remove(path);
}

// MARK: Graph

TEST(VrpGraph, Shape) {
    const vrp::Instance instance = tiny_instance();
    vrp::Graph graph;
    vrp::pricing_graph(instance, graph);

    // The depot, three customers and the sink; the depot to each customer, each customer to the
    // two others and to the sink.
    EXPECT_EQ(graph.get_number_of_nodes(), 5U);
    EXPECT_EQ(graph.get_number_of_arcs(), 3U + (3U * 3U));
}

TEST(VrpGraph, SubproblemWithZeroDuals) {
    const vrp::Instance instance = tiny_instance();
    vrp::Graph graph;
    vrp::pricing_graph(instance, graph);
    const rcspp::SolveResult result = solve(graph);

    ASSERT_FALSE(result.solutions.empty());
    for (const rcspp::Solution& route : result.solutions) {
        EXPECT_EQ(route.path_node_ids.front(), 0U);
        EXPECT_EQ(route.path_node_ids.back(), vrp::sink(instance));
        EXPECT_GE(route.cost, 0.0);
    }
}

TEST(VrpGraph, DualsLowerTheReducedCosts) {
    const vrp::Instance instance = tiny_instance();
    vrp::Graph graph;
    vrp::pricing_graph(instance, graph);
    const double best = solve(graph).solutions.front().cost;

    // One dual per master row, row i - 1 for customer i.
    std::mt19937 generator(25);  // a fixed seed, for a repeatable test
    std::uniform_real_distribution<double> dual(1.0, 100.0);
    std::vector<double> duals{dual(generator), dual(generator), dual(generator)};
    graph.update_reduced_costs(duals);

    EXPECT_LE(solve(graph).solutions.front().cost, best - 1e-9);
}

TEST(VrpGraph, TimeWindows) {
    // A customer that cannot be reached before its due time is on no route.
    const vrp::Instance instance{
        .name = "windows",
        .vehicles = 1,
        .capacity = 100.0,
        .customers = {depot(), customer(1, 1.0, 0.0), customer(2, 5.0, 0.0, 0.0)}};
    vrp::Graph graph;
    vrp::pricing_graph(instance, graph);

    for (const rcspp::Solution& route : solve(graph).solutions) {
        EXPECT_EQ(std::ranges::count(route.path_node_ids, 2U), 0);
    }
}

TEST(VrpGraph, Capacity) {
    // With room for one customer, no route visits two.
    const vrp::Instance instance = tiny_instance(15.0);
    vrp::Graph graph;
    vrp::pricing_graph(instance, graph);
    const rcspp::SolveResult result = solve(graph);

    ASSERT_FALSE(result.solutions.empty());
    for (const rcspp::Solution& route : result.solutions) {
        EXPECT_LE(customers_of(route, instance).size(), 1U);
    }
}

TEST(VrpGraph, SubproblemOnC101_5) {
    const vrp::Instance instance = vrp::read_solomon(vrp::find_instance("C101_5"));
    vrp::Graph graph;
    vrp::pricing_graph(instance, graph);
    const rcspp::SolveResult result = solve(graph);

    ASSERT_FALSE(result.solutions.empty());
    for (const rcspp::Solution& route : result.solutions) {
        EXPECT_EQ(route.path_node_ids.front(), 0U);
        EXPECT_EQ(route.path_node_ids.back(), vrp::sink(instance));
    }
}

TEST(VrpGraph, RouteMemoriesForbidRevisits) {
    // With every customer's duals high, the best route without memory revisits customers; with an
    // elementary memory, or ng memories holding every customer, none does.
    const vrp::Instance instance = tiny_instance(1000.0);
    for (const vrp::Relaxation relaxation :
         {vrp::Relaxation::Elementary, vrp::Relaxation::NgPath}) {
        vrp::Graph graph;
        vrp::pricing_graph(instance, graph, relaxation, instance.customers.size());
        graph.update_reduced_costs(std::vector<double>{100.0, 100.0, 100.0});

        for (const rcspp::Solution& route : solve(graph).solutions) {
            std::vector<size_t> customers = customers_of(route, instance);
            std::ranges::sort(customers);
            EXPECT_EQ(std::ranges::adjacent_find(customers), customers.end());
        }
    }

    vrp::Graph graph;
    vrp::pricing_graph(instance, graph);
    graph.update_reduced_costs(std::vector<double>{100.0, 100.0, 100.0});
    std::vector<size_t> customers = customers_of(solve(graph).solutions.front(), instance);
    std::ranges::sort(customers);
    EXPECT_NE(std::ranges::adjacent_find(customers), customers.end());
}

TEST(VrpGraph, ParseRelaxation) {
    EXPECT_EQ(vrp::parse_relaxation("none"), vrp::Relaxation::None);
    EXPECT_EQ(vrp::parse_relaxation("ng"), vrp::Relaxation::NgPath);
    EXPECT_EQ(vrp::parse_relaxation("elementary"), vrp::Relaxation::Elementary);
    EXPECT_FALSE(vrp::parse_relaxation("cycles").has_value());
}

// MARK: Column generation

TEST(VrpColumnGeneration, StartingRoutesCoverOneCustomerEach) {
    const std::vector<rcspp::Solution> routes = vrp::starting_routes(tiny_instance());

    ASSERT_EQ(routes.size(), 3U);
    for (size_t index = 0; index < routes.size(); ++index) {
        EXPECT_EQ(routes[index].path_node_ids, (std::vector<size_t>{0, index + 1, 4}));
        ASSERT_EQ(routes[index].column.rows.size(), 1U);
        EXPECT_EQ(routes[index].column.rows[0].index, index);
    }
    EXPECT_DOUBLE_EQ(routes[0].column.cost, 2.0);
}

TEST(VrpColumnGeneration, ParseMaster) {
    EXPECT_EQ(vrp::parse_master("gurobi"), vrp::Master::Gurobi);
    EXPECT_EQ(vrp::parse_master("highs"), vrp::Master::Highs);
    EXPECT_FALSE(vrp::parse_master("cplex").has_value());
}

TEST(VrpColumnGeneration, AMasterNotBuiltIsRefused) {
    for (const vrp::Master master : {vrp::Master::Gurobi, vrp::Master::Highs}) {
        if (std::ranges::count(built(), master) == 0) {
            EXPECT_THROW(static_cast<void>(vrp::master_solver(master, 3)), std::invalid_argument);
        }
    }
}

class VrpColumnGenerationOnEachMaster : public ::testing::TestWithParam<vrp::Master> {
    protected:
        void SetUp() override {
            if (GetParam() == vrp::Master::Gurobi && !gurobi_available()) {
                GTEST_SKIP() << "no Gurobi licence";
            }
        }
};

TEST_P(VrpColumnGenerationOnEachMaster, TinyInstance) {
    const vrp::Instance instance = tiny_instance();
    vrp::Graph graph;
    vrp::pricing_graph(instance, graph);
    vrp::ColumnGenerationModel model(instance, graph, GetParam());

    EXPECT_EQ(model.loop.run().status, cg::ColumnGenerationStatus::Optimal);

    const auto [status, routes] = vrp::integer_routes(model);
    ASSERT_EQ(status, cg::SolverStatus::Optimal);
    std::vector<size_t> visited;
    double total = 0.0;
    for (const vrp::Route& route : routes) {
        visited.insert(visited.end(), route.customers.begin(), route.customers.end());
        total += route.distance;
    }
    std::ranges::sort(visited);
    EXPECT_EQ(visited, (std::vector<size_t>{1, 2, 3}));
    EXPECT_NEAR(model.solver->objective(), total, 1e-9);
}

TEST_P(VrpColumnGenerationOnEachMaster, C101_25) {
    const vrp::Instance instance = vrp::read_solomon(vrp::find_instance("C101_25"));
    vrp::Graph graph;
    vrp::pricing_graph(instance, graph);
    size_t iterations = 0;
    vrp::ColumnGenerationOptions options;
    options.on_iteration = [&iterations](const cg::IterationInfo&) { ++iterations; };
    vrp::ColumnGenerationModel model(instance, graph, GetParam(), options);
    const cg::ColumnGenerationResult result = model.loop.run();

    EXPECT_EQ(result.status, cg::ColumnGenerationStatus::Optimal);
    EXPECT_NEAR(result.objective, c101_25_lp, c101_25_lp * 1e-6);
    EXPECT_EQ(iterations, result.iterations);

    // Every customer on exactly one route of the integer plan.
    const auto [status, routes] = vrp::integer_routes(model);
    ASSERT_EQ(status, cg::SolverStatus::Optimal);
    std::vector<size_t> visited;
    for (const vrp::Route& route : routes) {
        visited.insert(visited.end(), route.customers.begin(), route.customers.end());
    }
    std::ranges::sort(visited);
    std::vector<size_t> every(25);
    std::iota(every.begin(), every.end(), 1);
    EXPECT_EQ(visited, every);
}

TEST_P(VrpColumnGenerationOnEachMaster, MaxSolutionsCapsEachPricingCall) {
    const vrp::Instance instance = vrp::read_solomon(vrp::find_instance("C101_25"));
    vrp::Graph graph;
    vrp::pricing_graph(instance, graph);
    size_t most = 0;
    vrp::ColumnGenerationOptions options;
    options.max_solutions = 5;
    options.on_iteration = [&most](const cg::IterationInfo& info) {
        most = std::max(most, info.new_columns);
    };
    vrp::ColumnGenerationModel model(instance, graph, GetParam(), options);
    const cg::ColumnGenerationResult result = model.loop.run();

    EXPECT_EQ(result.status, cg::ColumnGenerationStatus::Optimal);
    EXPECT_NEAR(result.objective, c101_25_lp, c101_25_lp * 1e-6);
    EXPECT_LE(most, 5U);
}

TEST_P(VrpColumnGenerationOnEachMaster, NgRoutesGiveABoundAtLeastAsHigh) {
    const vrp::Instance instance = vrp::read_solomon(vrp::find_instance("R101_25"));
    std::map<vrp::Relaxation, double> bounds;
    for (const vrp::Relaxation relaxation : {vrp::Relaxation::None, vrp::Relaxation::NgPath}) {
        vrp::Graph graph;
        vrp::pricing_graph(instance, graph, relaxation);
        vrp::ColumnGenerationModel model(instance, graph, GetParam());
        const cg::ColumnGenerationResult result = model.loop.run();
        ASSERT_EQ(result.status, cg::ColumnGenerationStatus::Optimal);
        bounds[relaxation] = result.objective;
    }

    EXPECT_GE(bounds[vrp::Relaxation::NgPath], bounds[vrp::Relaxation::None] - 1e-6);
}

INSTANTIATE_TEST_SUITE_P(Built, VrpColumnGenerationOnEachMaster, ::testing::ValuesIn(built()),
                         [](const ::testing::TestParamInfo<vrp::Master>& info) {
                             return name_of(info.param);
                         });

}  // namespace
