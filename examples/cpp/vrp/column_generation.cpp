// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "vrp/column_generation.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "rcspp/column_generation.hpp"
#include "vrp/graph.hpp"
#include "vrp/instance.hpp"

#ifdef RCSPP_VRP_GUROBI
#include <gurobi_c++.h>

#include "rcspp/column_generation/gurobi/gurobi_master_solver.hpp"
#endif
#ifdef RCSPP_VRP_HIGHS
#include <Highs.h>

#include "rcspp/column_generation/highs/highs_master_solver.hpp"
#endif

namespace vrp {

namespace cg = rcspp::column_generation;

namespace {

#ifdef RCSPP_VRP_GUROBI
// A Gurobi model with one "= 1" row per customer, and the master solver over it.
struct GurobiMaster {
        static GRBEnv quiet_environment() {
            GRBEnv environment(true);
            environment.set(GRB_IntParam_OutputFlag, 0);
            environment.start();
            return environment;
        }

        static std::vector<GRBConstr> customer_rows(GRBModel& model, size_t customers) {
            std::vector<GRBConstr> rows;
            for (size_t customer = 1; customer <= customers; ++customer) {
                rows.push_back(model.addConstr(GRBLinExpr(), GRB_EQUAL, 1.0));
            }
            return rows;
        }

        explicit GurobiMaster(size_t customers)
            : environment(quiet_environment()),
              model(environment),
              rows(customer_rows(model, customers)),
              solver(model, rows) {}

        GRBEnv environment;
        GRBModel model;
        std::vector<GRBConstr> rows;
        cg::GurobiMasterSolver solver;
};
#endif

#ifdef RCSPP_VRP_HIGHS
// A HiGHS model with one "= 1" row per customer, and the master solver over it.
struct HighsMaster {
        static std::vector<HighsInt> customer_rows(Highs& highs, size_t customers) {
            highs.setOptionValue("output_flag", false);
            std::vector<HighsInt> rows;
            for (size_t customer = 1; customer <= customers; ++customer) {
                rows.push_back(highs.getNumRow());
                highs.addRow(1.0, 1.0, 0, nullptr, nullptr);
            }
            return rows;
        }

        explicit HighsMaster(size_t customers)
            : rows(customer_rows(highs, customers)), solver(highs, rows) {}

        Highs highs;
        std::vector<HighsInt> rows;
        cg::HighsMasterSolver solver;
};
#endif

// The pricing: rcspp's exact search, which proves that no route improves when it finds none.
cg::PricingStrategy<rcspp::RealResource, rcspp::SizeTBitsetResource> exact_search(
    std::optional<size_t> max_solutions) {
    using Stage = cg::PricingStage<rcspp::RealResource, rcspp::SizeTBitsetResource>;
    rcspp::AlgorithmParams<rcspp::LabelList<
        rcspp::ResourceTypeComposition<rcspp::RealResource, rcspp::SizeTBitsetResource>>>
        params;
    if (max_solutions.has_value()) {
        params.max_solutions = *max_solutions;
    }
    return cg::PricingStrategy<rcspp::RealResource, rcspp::SizeTBitsetResource>(
        std::vector<Stage>{Stage::make(params)});
}

// The loop's options: the starting routes make the first LP feasible, so no artificial column.
cg::ColumnGenerationParams loop_params(const ColumnGenerationOptions& options) {
    cg::ColumnGenerationParams params;
    params.timeout_s = options.timeout_s;
    params.initialization = std::make_shared<cg::NoInitialization>();
    params.on_iteration = options.on_iteration;
    return params;
}

}  // namespace

std::optional<Master> parse_master(std::string_view name) {
    if (name == "gurobi") {
        return Master::Gurobi;
    }
    if (name == "highs") {
        return Master::Highs;
    }
    return std::nullopt;
}

std::vector<std::string_view> built_masters() {
    std::vector<std::string_view> names;
#ifdef RCSPP_VRP_GUROBI
    names.emplace_back("gurobi");
#endif
#ifdef RCSPP_VRP_HIGHS
    names.emplace_back("highs");
#endif
    return names;
}

std::shared_ptr<cg::MasterSolver> master_solver(Master master, size_t customers) {
    switch (master) {
        case Master::Gurobi:
#ifdef RCSPP_VRP_GUROBI
            try {
                auto owner = std::make_shared<GurobiMaster>(customers);
                return {owner, &owner->solver};
            } catch (const GRBException& error) {  // no licence, for instance
                throw std::runtime_error("Gurobi: " + error.getMessage());
            }
#else
            throw std::invalid_argument(
                "master gurobi: build with USE_COLUMN_GENERATION_EXAMPLE_GUROBI=ON");
#endif
        case Master::Highs:
#ifdef RCSPP_VRP_HIGHS
        {
            auto owner = std::make_shared<HighsMaster>(customers);
            return {owner, &owner->solver};
        }
#else
            throw std::invalid_argument(
                "master highs: build with USE_COLUMN_GENERATION_EXAMPLE_HIGHS=ON");
#endif
    }
    throw std::invalid_argument("unknown master");
}

std::vector<rcspp::Solution> starting_routes(const Instance& instance) {
    const Customer& depot = instance.depot();
    std::vector<rcspp::Solution> routes;

    for (const Customer& customer : instance.customers) {
        if (customer.number == depot.number) {
            continue;
        }
        rcspp::Column column{
            .cost = 2 * distance(depot, customer),
            .rows = {rcspp::Row{.index = customer.number - 1, .coefficient = 1.0}}};
        routes.emplace_back(0.0,
                            std::vector<size_t>{depot.number, customer.number, sink(instance)},
                            std::vector<size_t>{},
                            std::move(column));
    }

    return routes;
}

ColumnGenerationModel::ColumnGenerationModel(const Instance& instance, Graph& graph, Master master,
                                             const ColumnGenerationOptions& options)
    : solver(master_solver(master, instance.customers.size() - 1)),
      problem(*solver),
      pricer(graph, exact_search(options.max_solutions)),
      loop(problem, loop_params(options)) {
    problem.add_columns(problem.add_pricer(pricer), starting_routes(instance));
}

std::pair<cg::SolverStatus, std::vector<Route>> integer_routes(ColumnGenerationModel& model,
                                                               double timeout_s) {
    const cg::SolverStatus status = model.loop.solve_restricted_master_integer(
        cg::IntegerParams{.timeout_s = timeout_s, .column_type = cg::ColumnType::Binary});
    if (status != cg::SolverStatus::Optimal && status != cg::SolverStatus::TimeLimit) {
        return {status, {}};
    }

    std::vector<const cg::MasterColumn*> columns;
    std::vector<cg::ColumnId> ids;
    for (const cg::MasterColumn& column : model.problem.columns()) {
        if (column.tag == cg::ColumnTag::Real) {
            columns.push_back(&column);
            ids.push_back(column.id);
        }
    }

    // Stopped at its time limit before finding a solution, the solver has no values.
    std::vector<double> values(ids.size());
    try {
        model.solver->primal(ids, values);
    } catch (const std::exception&) {  // GRBException is one too
        return {status, {}};
    }

    // A binary column's value is 0 or 1, give or take the solver's tolerance.
    constexpr double in_use = 0.5;
    std::vector<Route> routes;
    for (size_t index = 0; index < columns.size(); ++index) {
        if (values[index] > in_use) {
            const rcspp::Solution& solution = columns[index]->solution;
            const std::vector<size_t>& path = solution.path_node_ids;
            routes.push_back(
                Route{.customers = std::vector<size_t>(path.begin() + 1, path.end() - 1),
                      .distance = solution.column.cost});
        }
    }

    return {status, routes};
}

}  // namespace vrp
