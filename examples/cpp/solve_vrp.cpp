// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Solves the LP relaxation of a vehicle routing problem with time windows by column generation,
// then an integer plan over the routes it generated.
//
// Build it as examples/cpp/vrp/README.md says, then, from the repository's root:
//
//   rcspp-vrp instances/C101_25.txt --master highs
//   rcspp-vrp C101_25 --master gurobi --no-integer
//   rcspp-vrp R101_25 --master highs --relaxation ng --ng-size 8
//   rcspp-vrp C101_25 --subproblem-only

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <format>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "rcspp/column_generation.hpp"
#include "rcspp/rcspp.hpp"
#include "vrp/column_generation.hpp"
#include "vrp/graph.hpp"
#include "vrp/instance.hpp"

namespace {

namespace cg = rcspp::column_generation;

constexpr std::string_view usage =
    "usage: rcspp-vrp INSTANCE --master {gurobi,highs} [options]\n"
    "\n"
    "Column generation for a vehicle routing problem with time windows.\n"
    "\n"
    "  INSTANCE               a Solomon file, or its name in instances/ (C101_25)\n"
    "  --master M             the master's LP solver (required, unless --subproblem-only)\n"
    "  --max-solutions N      the most routes a pricing call returns (default: all)\n"
    "  --timeout S            column generation's time limit (default: none)\n"
    "  --integer-timeout S    the integer solve's time limit (default: 60)\n"
    "  --integer, --no-integer  solve the master with integer routes after the LP (default: yes)\n"
    "  --relaxation R         the routes priced: none, ng or elementary (default: none)\n"
    "  --ng-size N            with ng: each customer's nearest customers remembered (default: 8)\n"
    "  --subproblem-only      price once with zero duals and print the routes found: no master,\n"
    "                         no LP solver\n"
    "  --help                 this message\n";

// A wrong command line: its message, printed with the usage.
class UsageError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
};

struct Options {
        std::string instance;
        std::optional<vrp::Master> master;
        std::optional<size_t> max_solutions;
        double timeout_s = std::numeric_limits<double>::infinity();
        double integer_timeout_s = vrp::default_integer_timeout_s;
        bool integer = true;
        vrp::Relaxation relaxation = vrp::Relaxation::None;
        size_t ng_size = vrp::default_ng_size;
        bool subproblem_only = false;
};

std::string joined(const std::vector<std::string_view>& names) {
    std::string text;
    for (const std::string_view name : names) {
        text += text.empty() ? "" : " or ";
        text += name;
    }
    return text.empty() ? "none in this build" : text;
}

double parse_number(std::string_view option, const std::string& text) {
    try {
        size_t read = 0;
        const double value = std::stod(text, &read);
        if (read == text.size() && value >= 0.0) {
            return value;
        }
    } catch (const std::logic_error&) {  // not a number, or out of range
    }
    throw UsageError(std::format("{}: expected a number of 0 or more, not '{}'", option, text));
}

size_t parse_count(std::string_view option, const std::string& text) {
    try {
        size_t read = 0;
        const auto value = std::stoull(text, &read);
        if (read == text.size() && !text.starts_with('-')) {
            return static_cast<size_t>(value);
        }
    } catch (const std::logic_error&) {  // not a number, or out of range
    }
    throw UsageError(std::format("{}: expected a whole number, not '{}'", option, text));
}

vrp::Master parse_master_option(const std::string& name) {
    const std::optional<vrp::Master> master = vrp::parse_master(name);
    if (!master.has_value()) {
        throw UsageError(std::format("--master: '{}' is not gurobi or highs", name));
    }
    return *master;
}

vrp::Relaxation parse_relaxation_option(const std::string& name) {
    const std::optional<vrp::Relaxation> relaxation = vrp::parse_relaxation(name);
    if (!relaxation.has_value()) {
        throw UsageError(std::format("--relaxation: '{}' is not none, ng or elementary", name));
    }
    return *relaxation;
}

Options parse_arguments(const std::vector<std::string>& arguments) {
    Options options;
    for (size_t index = 0; index < arguments.size(); ++index) {
        const std::string& argument = arguments[index];

        // The value of an option that takes one.
        auto value = [&]() -> const std::string& {
            if (index + 1 == arguments.size()) {
                throw UsageError(std::format("{} needs a value", argument));
            }
            return arguments[++index];
        };

        if (argument == "--master") {
            options.master = parse_master_option(value());
        } else if (argument == "--max-solutions") {
            options.max_solutions = parse_count(argument, value());
        } else if (argument == "--timeout") {
            options.timeout_s = parse_number(argument, value());
        } else if (argument == "--integer-timeout") {
            options.integer_timeout_s = parse_number(argument, value());
        } else if (argument == "--integer") {
            options.integer = true;
        } else if (argument == "--no-integer") {
            options.integer = false;
        } else if (argument == "--relaxation") {
            options.relaxation = parse_relaxation_option(value());
        } else if (argument == "--ng-size") {
            options.ng_size = parse_count(argument, value());
        } else if (argument == "--subproblem-only") {
            options.subproblem_only = true;
        } else if (argument.starts_with("--")) {
            throw UsageError(std::format("unknown option {}", argument));
        } else if (options.instance.empty()) {
            options.instance = argument;
        } else {
            throw UsageError(std::format("one instance only, not '{}' too", argument));
        }
    }

    if (options.instance.empty()) {
        throw UsageError("an instance is required");
    }
    if (!options.master.has_value() && !options.subproblem_only) {
        throw UsageError(std::format("--master is required: {}", joined(vrp::built_masters())));
    }
    return options;
}

std::string_view status_name(cg::ColumnGenerationStatus status) {
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
        case cg::ColumnGenerationStatus::SolverError:
            return "SolverError";
    }
    return "Unknown";
}

std::string_view status_name(cg::SolverStatus status) {
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
    return "Unknown";
}

std::string path_text(const std::vector<size_t>& nodes) {
    std::string text = "[";
    for (const size_t node : nodes) {
        text += std::format("{}{}", text.size() > 1 ? ", " : "", node);
    }
    return text + "]";
}

double seconds_since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

void run(const Options& options) {
    const vrp::Instance instance = vrp::read_solomon(vrp::find_instance(options.instance));
    vrp::Graph graph;
    vrp::pricing_graph(instance, graph, options.relaxation, options.ng_size);
    std::cout << std::format("{}: {} customers, capacity {}, {} nodes, {} arcs\n",
                             instance.name,
                             instance.customers.size() - 1,
                             instance.capacity,
                             graph.get_number_of_nodes(),
                             graph.get_number_of_arcs());

    // Pricing alone: with zero duals, the reduced cost of a route is its distance.
    if (options.subproblem_only) {
        const rcspp::SolveResult result = graph.solve<rcspp::strategy::Simple>();
        std::cout << std::format("Routes found: {} (search {})\n",
                                 result.solutions.size(),
                                 rcspp::to_string(result.status));
        for (const rcspp::Solution& route : result.solutions) {
            std::cout << std::format("  {:10.4f}  {}\n",
                                     route.cost,
                                     path_text(route.path_node_ids));
        }
        return;
    }

    // Column generation, one line per iteration.
    vrp::ColumnGenerationOptions column_generation;
    column_generation.max_solutions = options.max_solutions;
    column_generation.timeout_s = options.timeout_s;
    column_generation.on_iteration = [](const cg::IterationInfo& info) {
        std::cout << std::format("  iteration {:4d}  LP {:14.4f}  +{:5d} routes  {:8.2f} s\n",
                                 info.iteration,
                                 info.objective,
                                 info.new_columns,
                                 info.elapsed_seconds);
    };

    vrp::ColumnGenerationModel model(instance, graph, *options.master, column_generation);
    auto start = std::chrono::steady_clock::now();
    const cg::ColumnGenerationResult result = model.loop.run();
    double seconds = seconds_since(start);
    const cg::Statistics& statistics = result.statistics;
    std::cout << std::format("{}: LP {:.4f}, lower bound {:.4f}\n",
                             status_name(result.status),
                             result.objective,
                             result.lower_bound);
    std::cout << std::format(
        "{} iterations, {} routes generated, {:.2f} s (master {:.2f} s, pricing {:.2f} s)\n",
        result.iterations,
        statistics.generated_columns,
        seconds,
        statistics.relaxation_seconds,
        statistics.pricing_seconds);

    if (!options.integer) {
        return;
    }

    // An integer plan over the routes generated.
    start = std::chrono::steady_clock::now();
    const auto [status, routes] = vrp::integer_routes(model, options.integer_timeout_s);
    seconds = seconds_since(start);

    if (routes.empty()) {
        std::cout << std::format("Integer solve: {}, no solution ({:.2f} s)\n",
                                 status_name(status),
                                 seconds);
        return;
    }

    double total = 0.0;
    for (const vrp::Route& route : routes) {
        total += route.distance;
    }
    std::cout << std::format("Integer solve: {}, {} routes, distance {:.4f} ({:.2f} s)\n",
                             status_name(status),
                             routes.size(),
                             total,
                             seconds);
    for (const vrp::Route& route : routes) {
        std::cout << std::format("  {:10.4f}  {}\n", route.distance, path_text(route.customers));
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    const std::vector<std::string> arguments(argv + 1, argv + argc);
    for (const std::string& argument : arguments) {
        if (argument == "--help" || argument == "-h") {
            std::cout << usage;
            return 0;
        }
    }

    try {
        run(parse_arguments(arguments));
    } catch (const UsageError& error) {
        std::cerr << usage << "\nrcspp-vrp: " << error.what() << '\n';
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "rcspp-vrp: " << error.what() << '\n';
        return 1;
    }

    // HiGHS's worker threads live until the process exits, and on Windows the exit can end
    // before std::cout's buffer is written: flush it while the program still runs.
    std::cout.flush();
    return 0;
}
