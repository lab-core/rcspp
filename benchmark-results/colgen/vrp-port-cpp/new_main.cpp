// MEASUREMENT ONLY. The same VRP as old_main.cpp on the C++ column generation example layer
// (examples/column_generation): the old example's graph (same resources, nodes, arcs and arc
// order; reduced costs by update_reduced_costs), except that the arc out of customer i covers
// master row i - 1 and arcs out of the depot cover none; a Gurobi model with one "= 1" row per
// customer wrapped in GurobiMasterSolver; GraphPricer with its default stage (exact
// SimpleDominanceAlgorithm, default parameters); single-customer starting columns,
// NoInitialization; ColumnGeneration with its defaults. A wrapper times GraphPricer::price apart
// from the master's add_columns. Prints one JSON line.
//
// Usage: new-vrp <instance file>

#include <gurobi_c++.h>

#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "instance_reader.hpp"
#include "rcspp/column_generation.hpp"
#include "rcspp/column_generation/gurobi/gurobi_master_solver.hpp"
#include "rcspp/rcspp.hpp"

#include "layer_vrp.hpp"

using namespace layer_vrp;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: new-vrp <instance file>\n");
        return 2;
    }
    Logger::init(LogLevel::Warn);

    Timer total(true);
    Timer graph_timer(true);
    InstanceReader reader(argv[1]);
    const auto instance = reader.read();
    VrpGraph graph;
    build_graph(instance, graph);
    graph_timer.stop();

    Timer setup(true);
    GRBEnv env(true);
    env.set(GRB_IntParam_OutputFlag, 0);
    env.start();
    GRBModel model(env);
    const auto& customers = instance.get_customers_by_id();
    std::vector<GRBConstr> rows;
    for (size_t id = 1; id < customers.size(); ++id) {
        rows.push_back(model.addConstr(GRBLinExpr(), GRB_EQUAL, 1.0, "c_" + std::to_string(id)));
    }
    GurobiMasterSolver solver(model, rows);
    MasterProblem master(solver);
    VrpPricer graph_pricer(graph);
    TimedPricer pricer(graph_pricer);
    const size_t index = master.add_pricer(pricer);

    auto routes = single_customer_routes(instance);
    master.add_columns(index, std::move(routes));

    ColumnGenerationParams params;
    params.initialization = std::make_shared<NoInitialization>();
    ColumnGeneration loop(master, params);
    setup.stop();

    Timer loop_timer(true);
    const ColumnGenerationResult result = loop.run();
    loop_timer.stop();
    total.stop();

    const auto& s = result.statistics;
    const double search = pricer.seconds();
    const double add = s.pricing_seconds - search;
    const double rest = loop_timer.elapsed_seconds() - s.relaxation_seconds - s.pricing_seconds;
    std::printf(
        "RESULT {\"side\": \"new\", \"status\": %d, \"lp\": %.12f, \"iterations\": %zu, "
        "\"columns\": %zu, \"graph_s\": %.6f, \"setup_s\": %.6f, \"master_s\": %.6f, "
        "\"search_s\": %.6f, \"add_s\": %.6f, \"rest_s\": %.6f, \"loop_s\": %.6f, "
        "\"total_s\": %.6f}\n",
        static_cast<int>(result.status), result.objective, result.iterations, s.generated_columns,
        graph_timer.elapsed_seconds(), setup.elapsed_seconds(), s.relaxation_seconds, search, add,
        rest, loop_timer.elapsed_seconds(), total.elapsed_seconds());
    return 0;
}
