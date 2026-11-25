// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// One column generation run of the benchmark's cpp-native layer: the C++ column generation example
// (examples/column_generation) in a C++ program, without Python. runners/cpp_native_runner.py
// expands a run's options into this program's flags, starts it, reads the line
// "RESULT <json>" it prints and writes the run's folder; the program writes nothing else.
//
// It builds the instance as the benchmark's SolomonInstance does
// (column_generation_benchmark/instances.py), so that every layer solves the same model: the
// nodes, the arcs and their order, three real resources (distance, time within the windows, load
// within the capacity), row 0 free and row i covered by the arcs out of customer i.
//
//   cpp-native-solver --instance instances/C101_25.txt --master highs --stage simple
//       --start single-customer --integer --solution
//
// Flags (each at most once unless said):
//   --instance PATH                 a Solomon file
//   --master highs|gurobi           the master solver
//   --master-option NAME KIND VALUE a solver parameter; KIND: bool, int, double or string; repeated
//   --stage ALGORITHM               a pricing stage: simple, pushing, pulling, astar, greedy,
//                                   tabu; repeated, in order (default: one simple)
//   --stage-param NAME VALUE        a field of the last stage's AlgorithmParams; a set as a comma
//                                   list ("" for none); repeated
//   --start single-customer|artificial  with --artificial-cost-factor F for artificial
//   --vehicles N                    the master row "at most N routes" (default: none)
//   --max-iterations N, --timeout S, --threads N
//   --absolute-gap G, --relative-gap G, --cutoff C, --integer-objective   the early stops
//   --tolerances absolute|relative  with --tolerance NAME VALUE (reduced_cost, feasibility,
//                                   optimality_gap), repeated
//   --integer                       solve the integer master over the generated columns, with
//                                   --integer-timeout S, --integer-gap G,
//                                   --integer-column-type binary|integer, --integer-threads N
//   --solution                      read the last LP's solution
//   --check                         build everything, solve nothing
//
// Exit codes: 0; 2 for wrong flags or a model that cannot be built (message on stderr); 1 for an
// error while solving.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "rcspp/column_generation.hpp"
#include "rcspp/rcspp.hpp"

#ifdef RCSPP_CPP_NATIVE_GUROBI
#include <gurobi_c++.h>

#include "rcspp/column_generation/gurobi/gurobi_master_solver.hpp"
#endif
#ifdef RCSPP_CPP_NATIVE_HIGHS
#include <Highs.h>

#include "rcspp/column_generation/highs/highs_master_solver.hpp"
#endif

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// After windows.h.
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

#ifndef RCSPP_BUILD_CONFIG
#define RCSPP_BUILD_CONFIG ""
#endif

// In rcspp's namespace, for its names, and in an unnamed one inside it.
namespace rcspp {

namespace {

namespace cg = column_generation;

using PricingGraph = ResourceGraph<RealResource>;
using Labels = LabelList<ResourceTypeComposition<RealResource>>;
using Params = AlgorithmParams<Labels>;
using Stage = cg::PricingStage<RealResource>;

// A wrong flag or a model that cannot be built: exit code 2.
class OptionsError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
};

// MARK: JSON

// Characters below this one are written as escapes.
constexpr unsigned char first_printable = 0x20;

std::string json_string(std::string_view text) {
    std::string quoted = "\"";
    for (const char character : text) {
        switch (character) {
            case '"':
                quoted += "\\\"";
                break;
            case '\\':
                quoted += "\\\\";
                break;
            case '\n':
                quoted += "\\n";
                break;
            default:
                if (static_cast<unsigned char>(character) < first_printable) {
                    quoted += std::format("\\u{:04x}", static_cast<int>(character));
                } else {
                    quoted += character;
                }
        }
    }
    return quoted + "\"";
}

// A number as JSON has it: null when it is not finite, else as many digits as it takes to read
// it back exactly.
std::string json_number(double value) {
    return std::isfinite(value) ? std::format("{}", value) : "null";
}

// A JSON object written key by key.
class JsonObject {
    public:
        JsonObject& add(std::string_view key, const std::string& json) {
            text_ += std::format("{}{}: {}", text_.empty() ? "" : ", ", json_string(key), json);
            return *this;
        }

        [[nodiscard]] std::string str() const { return "{" + text_ + "}"; }

    private:
        std::string text_;
};

template <typename Item, typename Function>
std::string json_list(const std::vector<Item>& items, Function&& to_json) {
    std::string text = "[";
    for (size_t index = 0; index < items.size(); ++index) {
        text += (index == 0 ? "" : ", ") + to_json(items[index]);
    }
    return text + "]";
}

// MARK: Options

struct StageOptions {
        std::string algorithm;
        std::vector<std::pair<std::string, std::string>> params;
};

struct MasterOption {
        std::string name;
        std::string kind;
        std::string value;
};

// The integer solve's time limit when the runner gives none, in seconds.
constexpr double default_integer_timeout_s = 60.0;

struct Options {
        std::string instance;
        std::string master;
        std::vector<MasterOption> master_options;
        std::vector<StageOptions> stages;
        std::string start = "artificial";
        std::optional<double> artificial_cost_factor;
        std::optional<size_t> max_iterations;
        std::optional<size_t> vehicles;
        std::optional<double> timeout_s;
        std::optional<int> threads;
        std::optional<double> absolute_gap;
        std::optional<double> relative_gap;
        std::optional<double> cutoff;
        bool integer_objective = false;
        std::optional<std::string> tolerances;
        std::map<std::string, double> tolerance_values;
        bool integer = false;
        double integer_timeout_s = default_integer_timeout_s;
        std::optional<double> integer_gap;
        std::string integer_column_type = "binary";
        std::optional<int> integer_threads;
        bool solution = false;
        bool check = false;
};

double to_double(std::string_view what, const std::string& text) {
    if (text == "inf") {
        return std::numeric_limits<double>::infinity();
    }
    try {
        size_t read = 0;
        const double value = std::stod(text, &read);
        if (read == text.size()) {
            return value;
        }
    } catch (const std::logic_error&) {  // not a number, or out of range
    }
    throw OptionsError(std::format("{}: '{}' is not a number", what, text));
}

std::int64_t to_integer(std::string_view what, const std::string& text) {
    try {
        size_t read = 0;
        const std::int64_t value = std::stoll(text, &read);
        if (read == text.size()) {
            return value;
        }
    } catch (const std::logic_error&) {  // not a number, or out of range
    }
    throw OptionsError(std::format("{}: '{}' is not a whole number", what, text));
}

size_t to_count(std::string_view what, const std::string& text) {
    const std::int64_t value = to_integer(what, text);
    if (value < 0) {
        throw OptionsError(std::format("{}: '{}' is negative", what, text));
    }
    return static_cast<size_t>(value);
}

bool to_bool(std::string_view what, const std::string& text) {
    if (text == "true") {
        return true;
    }
    if (text == "false") {
        return false;
    }
    throw OptionsError(std::format("{}: '{}' is not true or false", what, text));
}

Options parse_options(const std::vector<std::string>& arguments) {
    Options options;
    size_t index = 0;
    std::string flag;

    // The value that follows the flag.
    auto value = [&]() -> const std::string& {
        if (index + 1 >= arguments.size()) {
            throw OptionsError(std::format("{} needs a value", flag));
        }
        return arguments[++index];
    };

    const std::map<std::string, std::function<void()>> handlers = {
        {"--instance", [&] { options.instance = value(); }},
        {"--master", [&] { options.master = value(); }},
        {"--master-option",
         [&] {
             MasterOption option;
             option.name = value();
             option.kind = value();
             option.value = value();
             options.master_options.push_back(option);
         }},
        {"--stage",
         [&] { options.stages.push_back(StageOptions{.algorithm = value(), .params = {}}); }},
        {"--stage-param",
         [&] {
             if (options.stages.empty()) {
                 throw OptionsError("--stage-param comes after its --stage");
             }
             std::string name = value();
             options.stages.back().params.emplace_back(std::move(name), value());
         }},
        {"--start", [&] { options.start = value(); }},
        {"--artificial-cost-factor",
         [&] { options.artificial_cost_factor = to_double(flag, value()); }},
        {"--max-iterations", [&] { options.max_iterations = to_count(flag, value()); }},
        {"--vehicles", [&] { options.vehicles = to_count(flag, value()); }},
        {"--timeout", [&] { options.timeout_s = to_double(flag, value()); }},
        {"--threads", [&] { options.threads = static_cast<int>(to_integer(flag, value())); }},
        {"--absolute-gap", [&] { options.absolute_gap = to_double(flag, value()); }},
        {"--relative-gap", [&] { options.relative_gap = to_double(flag, value()); }},
        {"--cutoff", [&] { options.cutoff = to_double(flag, value()); }},
        {"--integer-objective", [&] { options.integer_objective = true; }},
        {"--tolerances", [&] { options.tolerances = value(); }},
        {"--tolerance",
         [&] {
             std::string name = value();
             options.tolerance_values[name] = to_double(flag, value());
         }},
        {"--integer", [&] { options.integer = true; }},
        {"--integer-timeout", [&] { options.integer_timeout_s = to_double(flag, value()); }},
        {"--integer-gap", [&] { options.integer_gap = to_double(flag, value()); }},
        {"--integer-column-type", [&] { options.integer_column_type = value(); }},
        {"--integer-threads",
         [&] { options.integer_threads = static_cast<int>(to_integer(flag, value())); }},
        {"--solution", [&] { options.solution = true; }},
        {"--check", [&] { options.check = true; }},
    };

    for (; index < arguments.size(); ++index) {
        flag = arguments[index];
        const auto handler = handlers.find(flag);
        if (handler == handlers.end()) {
            throw OptionsError(std::format("unknown flag {}", flag));
        }
        handler->second();
    }

    if (options.instance.empty() || options.master.empty()) {
        throw OptionsError("--instance and --master are required");
    }
    if (options.stages.empty()) {
        options.stages.push_back(StageOptions{.algorithm = "simple", .params = {}});
    }
    return options;
}

// MARK: Instance

struct Customer {
        size_t number = 0;
        double x = 0.0;
        double y = 0.0;
        double demand = 0.0;
        double ready_time = 0.0;
        double due_time = 0.0;
        double service_time = 0.0;
};

struct Instance {
        double capacity = 0.0;
        std::vector<Customer> customers;

        [[nodiscard]] size_t sink() const { return customers.size(); }
};

// A customer's line: number, x, y, demand, ready time, due time, service time.
constexpr size_t customer_fields = 7;

// The number of vehicles and their capacity: the first line with two numbers after the title;
// then every line with seven numbers is a customer.
Instance read_solomon(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw OptionsError(std::format("instance {}: cannot be read", path));
    }

    Instance instance;
    bool vehicles_read = false;
    std::string line;
    std::getline(file, line);
    while (std::getline(file, line)) {
        std::istringstream fields(line);
        std::vector<double> numbers;
        std::string field;
        bool all_numbers = true;
        while (all_numbers && fields >> field) {
            try {
                size_t read = 0;
                numbers.push_back(std::stod(field, &read));
                all_numbers = read == field.size();
            } catch (const std::logic_error&) {  // not a number
                all_numbers = false;
            }
        }
        if (!all_numbers) {
            continue;
        }

        if (numbers.size() == 2 && !vehicles_read) {
            instance.capacity = numbers[1];
            vehicles_read = true;
        } else if (numbers.size() == customer_fields) {
            std::istringstream values(line);
            double number = 0.0;
            Customer customer;
            values >> number >> customer.x >> customer.y >> customer.demand >>
                customer.ready_time >> customer.due_time >> customer.service_time;
            customer.number = static_cast<size_t>(number);
            instance.customers.push_back(customer);
        }
    }

    if (!vehicles_read || instance.customers.empty()) {
        throw OptionsError(std::format("instance {}: no vehicles or no customers", path));
    }
    for (size_t index = 0; index < instance.customers.size(); ++index) {
        if (instance.customers[index].number != index) {
            throw OptionsError(
                std::format("instance {}: customers must be numbered 0, 1, 2, ...", path));
        }
    }
    return instance;
}

double distance_between(const Customer& first, const Customer& second) {
    return std::hypot(second.x - first.x, second.y - first.y);
}

// As SolomonInstance.build_graph: the depot is the source, the sink comes after it, the empty
// route (depot to sink) is left out.
void build_graph(const Instance& instance, PricingGraph& graph) {
    std::map<size_t, std::pair<double, double>> windows;
    for (const Customer& customer : instance.customers) {
        windows[customer.number] = {customer.ready_time, customer.due_time};
    }
    // The sink is the depot again: a route is back before it closes.
    windows[instance.sink()] = windows[instance.customers.front().number];

    graph.add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                     std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                     std::make_unique<ValueCostFunction<RealResource>>(),
                                     std::make_unique<ValueDominanceFunction<RealResource>>());
    graph.add_resource<RealResource>(
        std::make_unique<TimeWindowExtensionFunction<RealResource>>(windows),
        std::make_unique<TimeWindowFeasibilityFunction<RealResource>>(windows),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());
    graph.add_resource<RealResource>(
        std::make_unique<AdditionExtensionFunction<RealResource>>(),
        std::make_unique<MinMaxFeasibilityFunction<RealResource>>(0.0, instance.capacity),
        std::make_unique<TrivialCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());

    const Customer& depot = instance.customers.front();
    for (const Customer& customer : instance.customers) {
        graph.add_node(customer.number, customer.number == depot.number);
        if (customer.number == depot.number) {
            graph.add_node(instance.sink(), false, true);
        }
    }

    for (const Customer& origin : instance.customers) {
        const bool from_depot = origin.number == depot.number;
        std::vector<Row> rows;
        if (!from_depot) {
            rows.push_back(Row{.index = origin.number, .coefficient = 1.0});
        }

        std::vector<std::pair<size_t, const Customer*>> targets;
        for (size_t index = 1; index < instance.customers.size(); ++index) {
            targets.emplace_back(instance.customers[index].number, &instance.customers[index]);
        }
        if (!from_depot) {
            targets.emplace_back(instance.sink(), &depot);
        }

        for (const auto& [node, destination] : targets) {
            if (destination == &origin) {
                continue;
            }
            const double length = distance_between(origin, *destination);
            graph.add_arc<RealResource, RealResource, RealResource>(
                {length, origin.service_time + length, destination->demand},
                origin.number,
                node,
                length,
                rows);
        }
    }
}

// As SolomonInstance.starting_columns: depot, customer, depot, at twice the distance.
std::vector<Solution> single_customer_routes(const Instance& instance) {
    const Customer& depot = instance.customers.front();
    std::vector<Solution> routes;
    for (size_t index = 1; index < instance.customers.size(); ++index) {
        const Customer& customer = instance.customers[index];
        Column column{.cost = 2 * std::hypot(customer.x - depot.x, customer.y - depot.y),
                      .rows = {Row{.index = customer.number, .coefficient = 1.0}}};
        routes.emplace_back(0.0,
                            std::vector<size_t>{depot.number, customer.number, instance.sink()},
                            std::vector<size_t>{},
                            std::move(column));
    }
    return routes;
}

// MARK: Pricing

std::set<size_t> to_set(const std::string& text) {
    std::set<size_t> values;
    std::istringstream items(text);
    std::string item;
    while (std::getline(items, item, ',')) {
        if (!item.empty()) {
            values.insert(to_count("a set's member", item));
        }
    }
    return values;
}

SearchDirection to_direction(const std::string& text) {
    if (text == "forward") {
        return SearchDirection::Forward;
    }
    if (text == "backward") {
        return SearchDirection::Backward;
    }
    if (text == "bidirectional") {
        return SearchDirection::Bidirectional;
    }
    throw OptionsError(
        std::format("direction '{}' is not forward, backward or bidirectional", text));
}

// The fields of AlgorithmParams that Python's AlgorithmParams can set, by name.
void set_param(Params& params, const std::string& name, const std::string& value) {
    const std::string what = "stage parameter " + name;
    const std::map<std::string, std::function<void()>> setters = {
        {"max_solutions", [&] { params.max_solutions = to_count(what, value); }},
        {"return_dominated_solutions",
         [&] { params.return_dominated_solutions = to_bool(what, value); }},
        {"prune_by_incumbent", [&] { params.prune_by_incumbent = to_bool(what, value); }},
        {"max_extended_labels_per_node",
         [&] { params.max_extended_labels_per_node = to_count(what, value); }},
        {"max_phases", [&] { params.max_phases = to_count(what, value); }},
        {"max_iterations", [&] { params.max_iterations = to_count(what, value); }},
        {"timeout_s", [&] { params.timeout_s = to_double(what, value); }},
        {"tolerance", [&] { params.tolerance = to_double(what, value); }},
        {"release_after_solve", [&] { params.release_after_solve = to_bool(what, value); }},
        {"tabu_tenure", [&] { params.tabu_tenure = to_count(what, value); }},
        {"tabu_exempt_node_ids", [&] { params.tabu_exempt_node_ids = to_set(value); }},
        {"tabu_random_noise", [&] { params.tabu_random_noise = to_bool(what, value); }},
        {"seed", [&] { params.seed = static_cast<int>(to_integer(what, value)); }},
        {"max_memory_gb", [&] { params.max_memory_gb = to_double(what, value); }},
        {"limit_to_available_ram", [&] { params.limit_to_available_ram = to_bool(what, value); }},
        {"limit_to_total_ram", [&] { params.limit_to_total_ram = to_bool(what, value); }},
        {"memory_limit_fraction", [&] { params.memory_limit_fraction = to_double(what, value); }},
        {"memory_check_interval", [&] { params.memory_check_interval = to_count(what, value); }},
        {"memory_pressure_fraction",
         [&] { params.memory_pressure_fraction = to_double(what, value); }},
        {"memory_pressure_max_labels_per_node",
         [&] { params.memory_pressure_max_labels_per_node = to_count(what, value); }},
        {"direction", [&] { params.direction = to_direction(value); }},
        {"critical_resource_index",
         [&] { params.critical_resource_index = to_count(what, value); }},
        {"half_way_point", [&] { params.half_way_point = to_double(what, value); }},
        {"join_after_early_stop", [&] { params.join_after_early_stop = to_bool(what, value); }},
    };
    const auto setter = setters.find(name);
    if (setter == setters.end()) {
        throw OptionsError(std::format("{} is not a field of AlgorithmParams", name));
    }
    setter->second();
}

// As the example's bindings make a stage (python/bindings/graph_impl.hpp): the algorithm by name,
// greedy and tabu heuristic, the others exact.
Stage make_stage(const StageOptions& stage) {
    Params params;
    for (const auto& [name, value] : stage.params) {
        set_param(params, name, value);
    }

    const std::string& algorithm = stage.algorithm;
    if (algorithm == "simple") {
        return Stage::make<strategy::Simple, Labels>(params, true);
    }
    if (algorithm == "pushing") {
        return Stage::make<strategy::Pushing, Labels>(params, true);
    }
    if (algorithm == "pulling") {
        return Stage::make<strategy::Pulling, Labels>(params, true);
    }
    if (algorithm == "astar") {
        return Stage::make<strategy::AStar, Labels>(params, true);
    }
    if (algorithm == "greedy") {
        return Stage::make<strategy::Greedy, Labels>(params, false);
    }
    if (algorithm == "tabu") {
        return Stage::make<strategy::Tabu, Labels>(params, false);
    }
    throw OptionsError(std::format(
        "algorithm '{}' is not one of simple, pushing, pulling, astar, greedy, tabu", algorithm));
}

// MARK: Masters

#ifdef RCSPP_CPP_NATIVE_GUROBI
// A Gurobi model of the instance's rows, and the master solver over it. Row 0 is free: a
// right-hand side no column total reaches, as the benchmark's gurobipy model has it.
struct GurobiMaster {
        static GRBEnv quiet_environment() {
            GRBEnv environment(true);
            environment.set(GRB_IntParam_OutputFlag, 0);
            environment.start();
            return environment;
        }

        static std::vector<GRBConstr> instance_rows(GRBModel& model, size_t customers) {
            constexpr double free_row = -1e30;
            std::vector<GRBConstr> rows{model.addConstr(GRBLinExpr(), GRB_GREATER_EQUAL, free_row)};
            for (size_t customer = 1; customer < customers; ++customer) {
                rows.push_back(model.addConstr(GRBLinExpr(), GRB_EQUAL, 1.0));
            }
            return rows;
        }

        explicit GurobiMaster(size_t customers)
            : environment(quiet_environment()),
              model(environment),
              rows(instance_rows(model, customers)),
              solver(model, rows) {}

        GRBEnv environment;
        GRBModel model;
        std::vector<GRBConstr> rows;
        cg::GurobiMasterSolver solver;
};
#endif

#ifdef RCSPP_CPP_NATIVE_HIGHS
// A HiGHS model of the instance's rows, and the master solver over it; row 0 free.
struct HighsMaster {
        static std::vector<HighsInt> instance_rows(Highs& highs, size_t customers) {
            highs.setOptionValue("output_flag", false);
            std::vector<HighsInt> rows;
            for (size_t customer = 0; customer < customers; ++customer) {
                rows.push_back(highs.getNumRow());
                if (customer == 0) {
                    highs.addRow(-kHighsInf, kHighsInf, 0, nullptr, nullptr);
                } else {
                    highs.addRow(1.0, 1.0, 0, nullptr, nullptr);
                }
            }
            return rows;
        }

        explicit HighsMaster(size_t customers)
            : rows(instance_rows(highs, customers)), solver(highs, rows) {}

        Highs highs;
        std::vector<HighsInt> rows;
        cg::HighsMasterSolver solver;
};
#endif

// The master solver; the pointer keeps its model alive.
std::shared_ptr<cg::MasterSolver> make_master_solver(const std::string& master, size_t customers) {
    if (master == "gurobi") {
#ifdef RCSPP_CPP_NATIVE_GUROBI
        try {
            auto owner = std::make_shared<GurobiMaster>(customers);
            return {owner, &owner->solver};
        } catch (const GRBException& error) {  // no licence, for instance
            throw OptionsError("master gurobi: " + error.getMessage());
        }
#else
        throw OptionsError("master gurobi needs USE_COLUMN_GENERATION_EXAMPLE_GUROBI=ON");
#endif
    }
    if (master == "highs") {
#ifdef RCSPP_CPP_NATIVE_HIGHS
        auto owner = std::make_shared<HighsMaster>(customers);
        return {owner, &owner->solver};
#else
        throw OptionsError("master highs needs USE_COLUMN_GENERATION_EXAMPLE_HIGHS=ON");
#endif
    }
    throw OptionsError(std::format("master '{}' is not highs or gurobi", master));
}

cg::OptionValue to_option_value(const MasterOption& option) {
    const std::string what = "master parameter " + option.name;
    if (option.kind == "bool") {
        return to_bool(what, option.value);
    }
    if (option.kind == "int") {
        return to_integer(what, option.value);
    }
    if (option.kind == "double") {
        return to_double(what, option.value);
    }
    if (option.kind == "string") {
        return option.value;
    }
    throw OptionsError(
        std::format("{}: kind '{}' is not bool, int, double or string", what, option.kind));
}

// MARK: Run

std::string status_name(cg::ColumnGenerationStatus status) {
    switch (status) {
        case cg::ColumnGenerationStatus::Optimal:
            return "Optimal";
        case cg::ColumnGenerationStatus::NotProven:
            return "NotProven";
        case cg::ColumnGenerationStatus::Infeasible:
            return "Infeasible";
        case cg::ColumnGenerationStatus::RestrictedMasterInfeasible:
            return "RestrictedMasterInfeasible";
        case cg::ColumnGenerationStatus::Unbounded:
            return "Unbounded";
        case cg::ColumnGenerationStatus::TimeLimit:
            return "TimeLimit";
        case cg::ColumnGenerationStatus::IterationLimit:
            return "IterationLimit";
        case cg::ColumnGenerationStatus::Cutoff:
            return "Cutoff";
        case cg::ColumnGenerationStatus::AbsoluteGapLimit:
            return "AbsoluteGapLimit";
        case cg::ColumnGenerationStatus::RelativeGapLimit:
            return "RelativeGapLimit";
        case cg::ColumnGenerationStatus::IntegerObjectiveGap:
            return "IntegerObjectiveGap";
        case cg::ColumnGenerationStatus::SolverError:
            return "SolverError";
    }
    return "SolverError";
}

std::string status_name(cg::SolverStatus status) {
    switch (status) {
        case cg::SolverStatus::Optimal:
            return "Optimal";
        case cg::SolverStatus::Infeasible:
            return "Infeasible";
        case cg::SolverStatus::Unbounded:
            return "Unbounded";
        case cg::SolverStatus::TimeLimit:
            return "TimeLimit";
        case cg::SolverStatus::Other:
            return "Other";
    }
    return "Other";
}

// Everything a run needs, built in place: the loop holds the master, the master its solver and
// pricer, the pricer the graph.
struct Model {
        explicit Model(const Options& options)
            : instance(read_solomon(options.instance)),
              solver(make_master_solver(options.master, instance.customers.size())),
              problem(*solver) {
            build_graph(instance, graph);
            for (const MasterOption& option : options.master_options) {
                try {
                    solver->set_option(option.name, to_option_value(option));
                } catch (const std::invalid_argument& error) {
                    throw OptionsError(std::format("master: {} refuses {} = {} ({})",
                                                   options.master,
                                                   option.name,
                                                   option.value,
                                                   error.what()));
                }
            }

            std::vector<Stage> stages;
            for (const StageOptions& stage : options.stages) {
                stages.push_back(make_stage(stage));
            }
            pricer.emplace(graph, cg::PricingStrategy<RealResource>(std::move(stages)));
            // The fleet row, when the run sets one: the pricer's convexity row.
            cg::ConvexityRow convexity;
            if (options.vehicles.has_value()) {
                convexity.max_paths = static_cast<double>(*options.vehicles);
            }
            const size_t index = problem.add_pricer(*pricer, convexity);

            cg::ColumnGenerationParams params;
            if (options.start == "artificial") {
                if (!options.artificial_cost_factor.has_value()) {
                    throw OptionsError("--start artificial needs --artificial-cost-factor");
                }
                params.initialization = std::make_shared<cg::ArtificialColumns>(
                    *options.artificial_cost_factor * std::max(1.0, pricer->cost_scale()));
            } else if (options.start == "single-customer") {
                problem.add_columns(index, single_customer_routes(instance));
                params.initialization = std::make_shared<cg::NoInitialization>();
            } else {
                throw OptionsError(
                    std::format("start '{}' is not artificial or single-customer", options.start));
            }
            if (options.max_iterations.has_value()) {
                params.max_iterations = *options.max_iterations;
            }
            if (options.timeout_s.has_value()) {
                params.timeout_s = *options.timeout_s;
            }
            if (options.tolerances.has_value()) {
                params.tolerances = make_tolerances(options);
            }
            if (options.threads.has_value()) {
                params.relaxation.threads = *options.threads;
            }
            params.absolute_gap = options.absolute_gap.value_or(params.absolute_gap);
            params.relative_gap = options.relative_gap.value_or(params.relative_gap);
            params.cutoff = options.cutoff.value_or(params.cutoff);
            params.integer_objective = options.integer_objective;
            loop.emplace(problem, params);
        }

        static std::shared_ptr<const cg::Tolerances> make_tolerances(const Options& options) {
            auto value = [&](const std::string& name, double fallback) {
                const auto found = options.tolerance_values.find(name);
                return found == options.tolerance_values.end() ? fallback : found->second;
            };
            for (const auto& [name, tolerance] : options.tolerance_values) {
                if (name != "reduced_cost" && name != "feasibility" && name != "optimality_gap") {
                    throw OptionsError(
                        std::format("tolerance '{}' is not reduced_cost, "
                                    "feasibility or optimality_gap",
                                    name));
                }
            }
            const double reduced_cost = value("reduced_cost", cg::default_reduced_cost_tolerance);
            const double feasibility = value("feasibility", cg::default_feasibility_tolerance);
            const double gap = value("optimality_gap", cg::default_optimality_gap);
            if (*options.tolerances == "absolute") {
                return std::make_shared<cg::AbsoluteTolerances>(reduced_cost, feasibility, gap);
            }
            if (*options.tolerances == "relative") {
                return std::make_shared<cg::RelativeTolerances>(reduced_cost, feasibility, gap);
            }
            throw OptionsError(
                std::format("tolerances '{}' is not absolute or relative", *options.tolerances));
        }

        Instance instance;
        PricingGraph graph;
        std::shared_ptr<cg::MasterSolver> solver;
        cg::MasterProblem problem;
        std::optional<cg::GraphPricer<RealResource>> pricer;
        std::optional<cg::ColumnGeneration> loop;
};

double seconds_since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

// The largest value a column has and still counts as unused.
constexpr double zero = 1e-9;

// The columns with a value, as solution.json lists them, and apart the artificial ones.
std::pair<std::string, std::string> describe_columns(const cg::MasterProblem& problem,
                                                     const std::vector<double>& values) {
    std::vector<std::string> used;
    std::vector<std::string> artificial;
    const auto columns = problem.columns();
    for (size_t index = 0; index < columns.size(); ++index) {
        const cg::MasterColumn& column = columns[index];
        const double value = values[index];
        if (std::abs(value) <= zero) {
            continue;
        }
        if (column.pricer == cg::MasterProblem::no_pricer) {
            artificial.push_back(JsonObject()
                                     .add("id", std::to_string(column.id))
                                     .add("value", json_number(value))
                                     .str());
            continue;
        }
        const Solution& solution = column.solution;
        const std::string path =
            json_list(solution.path_node_ids, [](size_t node) { return std::to_string(node); });
        const std::string rows = json_list(solution.column.rows, [](const Row& row) {
            return std::format("[{}, {}]",
                               row.index,
                               json_number(static_cast<double>(row.coefficient)));
        });
        used.push_back(JsonObject()
                           .add("id", std::to_string(column.id))
                           .add("value", json_number(value))
                           .add("cost", json_number(solution.column.cost))
                           .add("path", path)
                           .add("rows", rows)
                           .str());
    }
    auto list = [](const std::vector<std::string>& items) {
        return json_list(items, [](const std::string& item) { return item; });
    };
    return {list(used), list(artificial)};
}

std::vector<double> column_values(const Model& model) {
    std::vector<cg::ColumnId> ids;
    for (const cg::MasterColumn& column : model.problem.columns()) {
        ids.push_back(column.id);
    }
    std::vector<double> values(ids.size());
    model.solver->primal(ids, values);
    return values;
}

// The last LP's solution: its objective, the columns with a value and the duals.
std::string read_lp(const Model& model, double objective, bool solved_again) {
    const auto [used, artificial] = describe_columns(model.problem, column_values(model));
    std::vector<double> duals(model.problem.number_of_rows());
    model.solver->duals(duals);
    return JsonObject()
        .add("objective", json_number(objective))
        .add("solved_again", solved_again ? "true" : "false")
        .add("columns", used)
        .add("artificial_columns", artificial)
        .add("duals", json_list(duals, json_number))
        .str();
}

// The integer master over the generated columns: status, seconds, objective, columns.
std::string solve_integer(Model& model, const Options& options) {
    cg::IntegerParams params;
    params.timeout_s = options.integer_timeout_s;
    if (options.integer_gap.has_value()) {
        params.relative_gap = *options.integer_gap;
    }
    params.column_type =
        options.integer_column_type == "integer" ? cg::ColumnType::Integer : cg::ColumnType::Binary;
    if (options.integer_threads.has_value()) {
        params.threads = *options.integer_threads;
    } else if (options.threads.has_value()) {
        params.threads = *options.threads;
    }

    const auto start = std::chrono::steady_clock::now();
    const cg::SolverStatus status = model.loop->solve_restricted_master_integer(params);
    const double seconds = seconds_since(start);
    JsonObject found;
    found.add("status", json_string(status_name(status))).add("seconds", json_number(seconds));

    // Stopped at its limit, the solve may have no solution: the solver then has no values.
    try {
        const std::vector<double> values = column_values(model);
        const double objective = model.solver->objective();
        const auto [used, artificial] = describe_columns(model.problem, values);
        found.add("objective", json_number(objective))
            .add("columns", used)
            .add("artificial_columns", artificial);
    } catch (const std::exception&) {  // GRBException is one too
        found.add("objective", "null").add("columns", "[]").add("artificial_columns", "[]");
    }
    return found.str();
}

// MARK: Record

std::string versions_json() {
    JsonObject versions;
#ifdef RCSPP_CPP_NATIVE_HIGHS
    versions.add("highs", json_string(highsVersion()));
#endif
#ifdef RCSPP_CPP_NATIVE_GUROBI
    int major = 0;
    int minor = 0;
    int technical = 0;
    GRBversion(&major, &minor, &technical);
    versions.add("gurobi", json_string(std::format("{}.{}.{}", major, minor, technical)));
#endif
    return versions.str();
}

std::string build_json() {
#if defined(_MSC_VER) && !defined(__clang__)
    const std::string compiler = std::format("MSVC {}", _MSC_FULL_VER);
#elif defined(__clang__)
    const std::string compiler = std::format("Clang {}", __clang_version__);
#elif defined(__GNUC__)
    const std::string compiler = std::format("GCC {}", __VERSION__);
#else
    const std::string compiler = "unknown";
#endif
    // Empty for a single-configuration build without CMAKE_BUILD_TYPE.
    const char* const config = RCSPP_BUILD_CONFIG;
    return JsonObject()
        .add("config", std::string_view(config).empty() ? "null" : json_string(config))
        .add("compiler", json_string(compiler))
        .str();
}

// This process's peak memory, in bytes, and the processor time it used, in seconds.
std::string process_json() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS memory{};
    GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory));
    FILETIME created{};
    FILETIME ended{};
    FILETIME kernel{};
    FILETIME user{};
    GetProcessTimes(GetCurrentProcess(), &created, &ended, &kernel, &user);
    auto seconds = [](const FILETIME& time) {
        const auto ticks =
            (static_cast<std::uint64_t>(time.dwHighDateTime) << 32U) | time.dwLowDateTime;
        constexpr double ticks_per_second = 1e7;  // 100 ns
        return static_cast<double>(ticks) / ticks_per_second;
    };
    const auto peak = static_cast<std::uint64_t>(memory.PeakWorkingSetSize);
    const double processor = seconds(kernel) + seconds(user);
#else
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
#ifdef __APPLE__
    const auto peak = static_cast<std::uint64_t>(usage.ru_maxrss);  // bytes
#else
    constexpr std::uint64_t bytes_per_kilobyte = 1024;
    const auto peak = static_cast<std::uint64_t>(usage.ru_maxrss) * bytes_per_kilobyte;
#endif
    constexpr double microseconds = 1e-6;
    auto seconds = [](const timeval& time) {
        return static_cast<double>(time.tv_sec) +
               (static_cast<double>(time.tv_usec) * microseconds);
    };
    const double processor = seconds(usage.ru_utime) + seconds(usage.ru_stime);
#endif
    return JsonObject()
        .add("peak_memory_bytes", std::to_string(peak))
        .add("processor_seconds", json_number(processor))
        .str();
}

// MARK: Main

std::string run(const Options& options) {
    Model model(options);
    if (options.check) {
        return JsonObject()
            .add("built", "true")
            .add("versions", versions_json())
            .add("build", build_json())
            .str();
    }

    const auto start = std::chrono::steady_clock::now();
    const cg::ColumnGenerationResult result = model.loop->run();
    const double seconds = seconds_since(start);
    const cg::Statistics& statistics = result.statistics;
    const std::string status = status_name(result.status);
    const bool lp_solved = std::isfinite(result.objective);

    JsonObject fields;
    fields.add("status", json_string(status))
        .add("objective", json_number(result.objective))
        .add("lower_bound", json_number(result.lower_bound))
        .add("iterations", std::to_string(result.iterations))
        .add("columns", std::to_string(statistics.generated_columns))
        .add("artificial_columns", std::to_string(statistics.artificial_columns))
        .add("master_seconds", json_number(statistics.relaxation_seconds))
        .add("pricing_seconds", json_number(statistics.pricing_seconds))
        .add("other_seconds",
             json_number(seconds - statistics.relaxation_seconds - statistics.pricing_seconds))
        .add("solve_seconds", json_number(seconds));

    // Read before the integer solve, which replaces the LP's values. After a limit or a failed
    // solve, pricing may have added columns, which drop the values: the LP is solved again.
    std::string solution = "null";
    std::string solved_again_seconds = "null";
    if (options.solution) {
        const bool finished =
            status == "Optimal" || status == "NotProven" || status == "Infeasible";
        if (!lp_solved) {
            solution = R"({"lp": null, "lp_missing": "no master LP was solved"})";
        } else if (finished) {
            solution = JsonObject().add("lp", read_lp(model, result.objective, false)).str();
        } else {
            cg::RelaxationParams again;
            again.timeout_s = options.timeout_s.value_or(std::numeric_limits<double>::infinity());
            again.threads = options.threads.value_or(0);
            const auto start_again = std::chrono::steady_clock::now();
            const cg::SolverStatus solved = model.solver->solve_relaxation(again);
            model.solver->restore_user_model();
            solved_again_seconds = json_number(seconds_since(start_again));
            if (solved == cg::SolverStatus::Optimal) {
                solution =
                    JsonObject().add("lp", read_lp(model, model.solver->objective(), true)).str();
            } else {
                solution = JsonObject()
                               .add("lp", "null")
                               .add("lp_missing",
                                    json_string("the LP solved again ended " + status_name(solved)))
                               .str();
            }
        }
    }
    fields.add("lp_solved_again_seconds", solved_again_seconds);

    const std::string integer = options.integer ? solve_integer(model, options) : "null";
    return fields.add("solution", solution)
        .add("integer", integer)
        .add("versions", versions_json())
        .add("build", build_json())
        .add("process", process_json())
        .str();
}

}  // namespace

}  // namespace rcspp

int main(int argc, char* argv[]) {
    const std::vector<std::string> arguments(argv + 1, argv + argc);
    rcspp::Options options;
    try {
        options = rcspp::parse_options(arguments);
    } catch (const rcspp::OptionsError& error) {
        std::cerr << "cpp-native-solver: " << error.what() << '\n';
        return 2;
    }

    try {
        // Built whole first: an error must not leave a "RESULT" line behind.
        const std::string result = rcspp::run(options);
        // HiGHS's worker threads live until the process exits, and on Windows the exit can
        // end before std::cout's buffer is written: flush the result while the program runs.
        std::cout << "RESULT " << result << '\n' << std::flush;
    } catch (const rcspp::OptionsError& error) {
        std::cerr << "cpp-native-solver: " << error.what() << '\n';
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "cpp-native-solver: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
