// MEASUREMENT ONLY. Pricing on one dual sequence: the old example's loop runs as old_main.cpp
// does; at each iteration, outside its timers, the layer's GraphPricer prices the same duals on
// the layer's graph (rows shifted by one, threshold -1e-8 as the old example's), and a shadow
// layer master (GurobiMasterSolver over its own Gurobi model, never solved) receives the columns
// it found, as the loop would add them. Prints one JSON line with both sides' pricing and adding
// times over the same duals, and how often their columns differ.
//
// Usage: replay-vrp <instance file>

#include <gurobi_c++.h>

#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "instance_reader.hpp"
#include "layer_vrp.hpp"
#include "rcspp/column_generation/gurobi/gurobi_master_solver.hpp"
#include "vrp.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: replay-vrp <instance file>\n");
        return 2;
    }
    Logger::init(LogLevel::Warn);

    InstanceReader reader(argv[1]);
    const auto instance = reader.read();
    VRP vrp(instance);

    layer_vrp::VrpGraph graph;
    layer_vrp::build_graph(instance, graph);

    GRBEnv env(true);
    env.set(GRB_IntParam_OutputFlag, 0);
    env.start();
    GRBModel model(env);
    const size_t customers = instance.get_customers_by_id().size() - 1;
    std::vector<GRBConstr> rows;
    for (size_t id = 1; id <= customers; ++id) {
        rows.push_back(model.addConstr(GRBLinExpr(), GRB_EQUAL, 1.0, "c_" + std::to_string(id)));
    }
    layer_vrp::GurobiMasterSolver solver(model, rows);
    layer_vrp::MasterProblem master(solver);
    layer_vrp::VrpPricer graph_pricer(graph);
    const size_t index = master.add_pricer(graph_pricer);
    master.add_columns(index, layer_vrp::single_customer_routes(instance));

    Timer price_timer;
    Timer add_timer;
    size_t new_columns = 0;
    size_t count_differs = 0;
    size_t best_differs = 0;
    std::vector<double> duals(customers, 0.0);

    auto observer = [&](const std::map<size_t, double>& dual_by_id,
                        const std::vector<Solution>& old_solutions) {
        for (const auto& [id, value] : dual_by_id) {
            duals[id - 1] = value;
        }
        price_timer.start();
        layer_vrp::PricingResult priced =
            graph_pricer.price(duals, layer_vrp::PricingRequest{.reduced_cost_threshold = -1e-8});
        price_timer.stop();

        size_t old_count = 0;
        double old_best = 0.0;
        for (const auto& solution : old_solutions) {
            if (solution.cost < -1e-8) {
                ++old_count;
                old_best = std::min(old_best, solution.cost);
            }
        }
        double new_best = 0.0;
        for (const auto& column : priced.columns) {
            new_best = std::min(new_best, column.cost);
        }
        count_differs += old_count != priced.columns.size() ? 1 : 0;
        best_differs += std::abs(old_best - new_best) > 1e-6 ? 1 : 0;
        new_columns += priced.columns.size();

        add_timer.start();
        master.add_columns(index, std::move(priced.columns));
        add_timer.stop();
    };

    AlgorithmParams<LabelList<ResourceType>> params;
    const auto r = vrp.solve_timed(params, observer);

    std::printf(
        "RESULT {\"side\": \"replay\", \"lp\": %.12f, \"iterations\": %d, \"columns\": %zu, "
        "\"layer_columns\": %zu, \"count_differs\": %zu, \"best_differs\": %zu, "
        "\"old_search_s\": %.6f, \"layer_price_s\": %.6f, \"old_add_s\": %.6f, "
        "\"layer_add_s\": %.6f}\n",
        r.lp_cost, r.iterations, r.columns, new_columns, count_differs, best_differs, r.search_s,
        price_timer.elapsed_seconds(), r.add_s, add_timer.elapsed_seconds());
    return 0;
}
