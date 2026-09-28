// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Pricing cascades over a whole column generation: heuristic stages first, an exact one last.
//
// Each cascade prices every iteration by running its stages in order and stopping at the first
// stage after which `--min-columns` improving columns exist (VRP::solve_cascade). The final stage
// is exact, so a column generation that ends is proven optimal and every cascade must reach the
// same LP value -- the driver says so loudly when one does not.
//
// Cascades (select with `--cascades=A,C,...`; all by default):
//   A  exact forward (Simple)
//   B  exact bidirectional (static H = horizon / 2)
//   C  Greedy                                   -> exact forward
//   D  Greedy                                   -> exact bidirectional
//   E  truncated bidirectional (--quota)        -> exact bidirectional
//   F  ImprovingTabu -> truncated bidirectional -> exact bidirectional
//   G  truncated forward Pushing (--quota)      -> exact forward
//
// Other options:
//   --min-columns=K     improving columns that end an iteration's pricing early (default 1)
//   --quota=Q           per-node extension quota of the truncated stages (default 5)
//   --join-budget=K     join_column_budget of every bidirectional stage (default none)
//   --dive-iterations=N max_iterations of the Greedy / ImprovingTabu stages (default 200)
//
// Pass instance names to choose them (default `R101_25 R201_25`). Needs Gurobi, like every other
// column-generation driver here.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "vrp/instance_reader.hpp"
#include "vrp/vrp.hpp"

namespace {

using ListLC = LabelList<ResourceType>;
using AlgoPtr = std::unique_ptr<Algorithm<ResourceType, ListLC>>;

/// @brief The driver's options.
struct Options {
        size_t min_columns = 1;
        size_t quota = 5;
        size_t join_budget = MAX_INT;
        size_t dive_iterations = 200;
        std::string cascades = "ABCDEFG";
};

/// @brief Builds the algorithms a cascade's stages point at, and keeps them alive.
class StageFactory {
    public:
        StageFactory(VRP* vrp, double horizon, Options options)
            : vrp_(vrp), horizon_(horizon), options_(std::move(options)) {}

        PricingStage exact_forward() {
            return keep("exact forward",
                        make<SimpleDominanceAlgorithm>(AlgorithmBaseParams{}),
                        true);
        }

        PricingStage exact_bidirectional() {
            return keep("exact bidirectional",
                        make<SimpleDominanceAlgorithm>(bidirectional()),
                        true);
        }

        PricingStage truncated_bidirectional() {
            auto params = bidirectional();
            params.num_labels_to_extend_by_node = options_.quota;
            // Pushing's bidirectional search: the quota keeps each node's cheapest labels.
            return keep("bidirectional q" + std::to_string(options_.quota),
                        make<PushingDominanceAlgorithm>(params),
                        false);
        }

        PricingStage truncated_pushing() {
            AlgorithmBaseParams params;
            params.num_labels_to_extend_by_node = options_.quota;
            return keep("pushing q" + std::to_string(options_.quota),
                        make<PushingDominanceAlgorithm>(params),
                        false);
        }

        PricingStage greedy() {
            AlgorithmBaseParams params;
            params.stop_after_X_solutions = 20;  // NOLINT(readability-magic-numbers)
            params.max_iterations = options_.dive_iterations;
            params.timeout_s = kDiveGuardSeconds;
            return keep("greedy", make<GreedyAlgorithm>(params), false);
        }

        PricingStage improving_tabu() {
            AlgorithmBaseParams params;
            params.max_iterations = options_.dive_iterations;
            params.timeout_s = kDiveGuardSeconds;
            return keep("improving tabu", make<ImprovingTabuSearch>(params), false);
        }

    private:
        /// @brief A heuristic stage that runs away is stopped; it is a heuristic either way.
        static constexpr double kDiveGuardSeconds = 5.0;

        VRP* vrp_;
        double horizon_;
        Options options_;
        std::vector<AlgoPtr> owned_;

        [[nodiscard]] AlgorithmBaseParams bidirectional() const {
            AlgorithmBaseParams params;
            params.direction = SearchDirection::Bidirectional;
            params.critical_resource_index = 1;      // time: real slot 1, after the cost
            params.half_way_point = horizon_ / 2.0;  // NOLINT(readability-magic-numbers)
            params.join_column_budget = options_.join_budget;
            return params;
        }

        template <template <typename, typename> class Algo>
        AlgoPtr make(const AlgorithmBaseParams& params) {
            return vrp_->get_graph().create_algorithm<Algo, ListLC>(
                params.with_container(ListLC()));
        }

        PricingStage keep(std::string name, AlgoPtr algorithm, bool exact) {
            owned_.push_back(std::move(algorithm));
            return {.name = std::move(name), .algorithm = owned_.back().get(), .exact = exact};
        }
};

/// @brief The stages of cascade @p id.
std::vector<PricingStage> stages_of(char id, StageFactory* factory) {
    switch (id) {
        case 'A':
            return {factory->exact_forward()};
        case 'B':
            return {factory->exact_bidirectional()};
        case 'C':
            return {factory->greedy(), factory->exact_forward()};
        case 'D':
            return {factory->greedy(), factory->exact_bidirectional()};
        case 'E':
            return {factory->truncated_bidirectional(), factory->exact_bidirectional()};
        case 'F':
            return {factory->improving_tabu(),
                    factory->truncated_bidirectional(),
                    factory->exact_bidirectional()};
        case 'G':
            return {factory->truncated_pushing(), factory->exact_forward()};
        default:
            return {};
    }
}

struct CascadeRun {
        char id;
        CascadeResult result;
        double wall = 0.0;
};

void print(const CascadeRun& run) {
    const auto& result = run.result;
    double pricing = 0.0;
    for (const auto& stage : result.stages) {
        pricing += stage.seconds;
    }
    constexpr int kLpDigits = 10;
    constexpr int kCsvDigits = 12;
    constexpr int kNameWidth = 22;
    constexpr int kCountWidth = 4;
    constexpr int kColumnsWidth = 6;
    std::cout << "    [" << run.id << "] lp=" << std::setprecision(kLpDigits) << result.lp_cost
              << (result.proven_optimal ? "" : " (NOT proven)") << std::setprecision(4)
              << "  iterations=" << result.iterations << "  wall=" << run.wall
              << " s  pricing=" << pricing << " s  master=" << result.master_seconds << " s\n";
    for (const auto& stage : result.stages) {
        std::cout << "        " << std::left << std::setw(kNameWidth) << stage.name << std::right
                  << " calls=" << std::setw(kCountWidth) << stage.calls
                  << " successes=" << std::setw(kCountWidth) << stage.successes
                  << " columns=" << std::setw(kColumnsWidth) << stage.columns
                  << " seconds=" << stage.seconds << "\n";
    }
    // One machine-readable line per cascade, for tabulating.
    std::cout << "    [ CSV ] " << run.id << ',' << std::setprecision(kCsvDigits) << result.lp_cost
              << ',' << (result.proven_optimal ? 1 : 0) << ',' << result.iterations << ','
              << run.wall << ',' << pricing << ',' << result.master_seconds;
    for (const auto& stage : result.stages) {
        std::cout << ',' << stage.name << ':' << stage.calls << ':' << stage.successes << ':'
                  << stage.columns << ':' << stage.seconds;
    }
    std::cout << std::endl;
}

/// @brief Reads the command line: options as documented at the top, the rest instance names.
Options parse(int argc, char** argv, std::vector<std::string>* names) {
    Options options;
    const auto value_of = [](const std::string& arg) { return arg.substr(arg.find('=') + 1); };
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (arg.starts_with("--min-columns=")) {
            options.min_columns = std::stoull(value_of(arg));
        } else if (arg.starts_with("--quota=")) {
            options.quota = std::stoull(value_of(arg));
        } else if (arg.starts_with("--join-budget=")) {
            options.join_budget = std::stoull(value_of(arg));
        } else if (arg.starts_with("--dive-iterations=")) {
            options.dive_iterations = std::stoull(value_of(arg));
        } else if (arg.starts_with("--cascades=")) {
            options.cascades.clear();
            std::ranges::copy_if(value_of(arg), std::back_inserter(options.cascades), [](char c) {
                return c != ',';
            });
        } else {
            names->push_back(arg);
        }
    }
    if (names->empty()) {
        *names = {"R101_25", "R201_25"};
    }
    return options;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> names;
    const Options options = parse(argc, argv, &names);

    std::cout << "[ CASCADE ] min_columns=" << options.min_columns << "  quota=" << options.quota
              << "  join_budget="
              << (options.join_budget >= MAX_INT ? std::string("none")
                                                 : std::to_string(options.join_budget))
              << "  dive_iterations=" << options.dive_iterations
              << "  cascades=" << options.cascades << "\n"
              << std::endl;

    const std::string root_dir = file_parent_dir(__FILE__, 3);
    int disagreements = 0;
    for (const auto& name : names) {
        InstanceReader reader(root_dir + "/instances/" + name + ".txt");
        const auto instance = reader.read();
        const auto horizon = static_cast<double>(instance.get_depot_customer().due_time);
        std::cout << "  " << name << " (horizon " << horizon << ")" << std::endl;

        std::vector<CascadeRun> runs;
        for (const char id : options.cascades) {
            VRP vrp(instance);
            StageFactory factory(&vrp, horizon, options);
            const auto stages = stages_of(id, &factory);
            if (stages.empty()) {
                std::cout << "    unknown cascade '" << id << "'" << std::endl;
                continue;
            }
            const auto started = std::chrono::steady_clock::now();
            CascadeRun run{.id = id, .result = vrp.solve_cascade(stages, options.min_columns)};
            run.wall =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            print(run);
            runs.push_back(std::move(run));
        }

        // Every proven cascade prices the same LP to optimality, so they must agree.
        for (size_t i = 1; i < runs.size(); ++i) {
            const double gap = std::abs(runs[i].result.lp_cost - runs.front().result.lp_cost);
            if (gap > 1e-6 && runs[i].result.proven_optimal &&  // NOLINT(readability-magic-numbers)
                runs.front().result.proven_optimal) {
                std::cout << "    *** DISAGREEMENT: [" << runs.front().id << "] "
                          << runs.front().result.lp_cost << " vs [" << runs[i].id << "] "
                          << runs[i].result.lp_cost << std::endl;
                ++disagreements;
            }
        }
        std::cout << std::endl;
    }
    return disagreements == 0 ? 0 : 1;
}
