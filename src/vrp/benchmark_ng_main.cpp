// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.
//
// Benchmark for the ng-route unreachable-set dominance augmentation.
//
// For each instance and each ng-neighbourhood size, the column-generation LP
// bound and the pricing label count are reported with the augmentation OFF
// (baseline ng) and ON.  The augmentation is pure acceleration: lp_base must
// equal lp_aug while nlab_aug <= nlab_base (strictly less on tight-TW instances).
//
// Usage:  rcspp-vrp-benchmark-ng [max_instance_index] [--ng s1 s2 ...]
//                                [--max-labels N] [--cols K]
//   max_instance_index : 1..9, expands to C10i / R10i / RC10i (default 2)
//   --ng s1 s2 ...     : ng-neighbourhood sizes to sweep (default 3 5 8)
//   --max-labels N     : max labels expanded per node, for early stopping on the
//                        first (expensive) CG iterations (default 100)
//   --cols K           : columns added to the master per CG iteration (default:
//                        #demand customers); the labeling stops after 4*K solutions
//   --family F         : restrict to one instance family C / R / RC (default: all)
//   --instance NAME    : run a single named instance (e.g. C101); overrides the
//                        family/index selection above

#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "instance.hpp"
#include "instance_reader.hpp"
#include "rcspp/rcspp.hpp"
#include "vrp.hpp"

namespace {

// "[instance ng=N augmented]" style tag identifying one run in the log.
std::string run_tag(const std::string& instance_name, size_t ng_size, bool augment) {
    return "[" + instance_name + " ng=" + std::to_string(ng_size) + " " +
           (augment ? "augmented" : "baseline") + "]";
}

// Pricing parameters with early-stopping caps.  Exact ng-route pricing is ~O(n^3)
// in the number of customers, so the first CG iterations (large duals => almost
// every partial path has negative reduced cost) would otherwise explore millions
// of labels.  Two caps keep it tractable: expand at most `max_labels` labels per
// node, and stop the labeling after 4 x cols_per_iter solutions (4x the number of
// columns added to the master each iteration).  Pricing then becomes heuristic
// (the LP bound is a valid estimate, not exact), but baseline and augmented runs
// use identical caps, so the comparison stays fair.
AlgorithmParams<LabelList<ResourceType>> capped_params(size_t max_labels, size_t cols_per_iter) {
    AlgorithmParams<LabelList<ResourceType>> params;
    params.num_labels_to_extend_by_node = max_labels;
    params.stop_after_X_solutions = 4 * cols_per_iter;
    // stop_after_X_solutions only takes effect when solutions are collected during
    // the run (not just at the end), which requires return_dominated_solutions and
    // num_max_phases to be large to ensure optimality.
    params.return_dominated_solutions = true;
    params.num_max_phases = MAX_INT;
    return params;
}

struct CgStats {
        double lp_cost = std::numeric_limits<double>::infinity();
        size_t labels = 0;     // total labels extended across all pricing iterations
        double seconds = 0.0;  // wall-clock time of the CG run
};

// One column-generation run: returns the (capped, heuristic) LP bound and the
// total labels extended across all pricing iterations -- the labeling effort the
// augmentation is meant to reduce.
CgStats cg_run(const std::string& instance_name, const Instance& instance, size_t ng_size,
               bool augment, size_t max_labels, size_t cols_per_iter) {
    const std::string tag = run_tag(instance_name, ng_size, augment);
    LOG_INFO(tag, " CG solve start\n");
    Timer timer;
    timer.start();

    VRP vrp(instance, ng_size, augment);
    auto result = vrp.solve<SimpleDominanceAlgorithm>(capped_params(max_labels, cols_per_iter),
                                                      std::nullopt,
                                                      {},
                                                      /*run_boost=*/false,
                                                      /*extra_solvers=*/{},
                                                      /*max_columns_per_iter=*/cols_per_iter);

    timer.stop();
    LOG_INFO(tag,
             " CG solve done  lp=",
             result.lp_cost,
             " labels=",
             result.total_pricing_labels,
             " (",
             timer.elapsed_seconds(),
             "s)\n");
    return {result.lp_cost, result.total_pricing_labels, timer.elapsed_seconds()};
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        Logger::init(LogLevel::Info);

        size_t max_instance_index = 2;
        size_t max_labels = 100;    // per-node label-expansion cap (--max-labels)
        size_t cols_per_iter = 0;   // columns added per CG iteration; 0 => #customers
        std::string family;         // "C" / "R" / "RC"; empty => all (--family)
        std::string instance_name;  // single instance, e.g. "C101" (--instance)
        std::vector<size_t> ng_sizes;
        enum class Reading { kNone, kNg } reading = Reading::kNone;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--ng") {
                reading = Reading::kNg;
            } else if (arg == "--max-labels") {
                reading = Reading::kNone;
                max_labels = std::stoull(argv[++i]);
            } else if (arg == "--cols") {
                reading = Reading::kNone;
                cols_per_iter = std::stoull(argv[++i]);
            } else if (arg == "--family") {
                reading = Reading::kNone;
                family = argv[++i];
            } else if (arg == "--instance") {
                reading = Reading::kNone;
                instance_name = argv[++i];
            } else if (reading == Reading::kNg) {
                ng_sizes.push_back(std::stoull(arg));
            } else {
                max_instance_index = std::stoull(arg);
            }
        }
        if (ng_sizes.empty()) {
            ng_sizes = {3, 5, 8};  // NOLINT(readability-magic-numbers)
        }
        if (max_instance_index < 1 ||
            max_instance_index > 9) {  // NOLINT(readability-magic-numbers)
            LOG_ERROR("max_instance_index must be in 1..9\n");
            return 1;
        }
        if (!family.empty() && family != "C" && family != "R" && family != "RC") {
            LOG_ERROR("--family must be one of C, R, RC\n");
            return 1;
        }

        LOG_INFO("ng benchmark: max_labels=",
                 max_labels,
                 ", cols_per_iter=",
                 (cols_per_iter == 0 ? std::string("#customers") : std::to_string(cols_per_iter)),
                 ", stop_after_X_solutions=4*cols_per_iter\n");

        std::vector<std::string> instance_names;
        if (!instance_name.empty()) {
            instance_names.push_back(instance_name);  // single instance overrides family/index
        } else {
            for (size_t i = 1; i <= max_instance_index; ++i) {
                if (family.empty() || family == "C") {
                    instance_names.emplace_back("C10" + std::to_string(i));
                }
                if (family.empty() || family == "R") {
                    instance_names.emplace_back("R10" + std::to_string(i));
                }
                if (family.empty() || family == "RC") {
                    instance_names.emplace_back("RC10" + std::to_string(i));
                }
            }
        }

        const std::string root_dir = file_parent_dir(__FILE__, 3);

        std::ostringstream table;
        table << std::left << std::setw(8) << "Instance" << std::right << std::setw(5) << "ng"
              << std::setw(14) << "lp_base" << std::setw(14) << "lp_aug" << std::setw(12)
              << "nlab_base" << std::setw(12) << "nlab_aug" << std::setw(9) << "drop%"
              << std::setw(11) << "t_base" << std::setw(11) << "t_aug" << std::setw(9) << "t_drop%"
              << "\n";
        table << std::string(105, '-') << "\n";

        for (const auto& instance_name : instance_names) {
            const std::string instance_path = root_dir + "/instances/" + instance_name + ".txt";
            InstanceReader reader(instance_path);
            auto instance = reader.read();

            for (size_t ng_size : ng_sizes) {
                const std::string header =
                    "instance=" + instance_name + " ng_size=" + std::to_string(ng_size) + " (" +
                    std::to_string(instance.get_demand_customers_id().size()) + " customers)";
                LOG_INFO('\n',
                         std::string(70, '='),
                         "\nTest start: ",
                         header,
                         '\n',
                         std::string(70, '='),
                         '\n');
                Timer test_timer;
                test_timer.start();

                const size_t cols =
                    cols_per_iter == 0 ? instance.get_demand_customers_id().size() : cols_per_iter;
                const CgStats base =
                    cg_run(instance_name, instance, ng_size, false, max_labels, cols);
                const CgStats aug =
                    cg_run(instance_name, instance, ng_size, true, max_labels, cols);

                test_timer.stop();
                LOG_INFO("Test done : ",
                         header,
                         "  lp_base=",
                         base.lp_cost,
                         " lp_aug=",
                         aug.lp_cost,
                         " nlab_base=",
                         base.labels,
                         " nlab_aug=",
                         aug.labels,
                         "  (",
                         test_timer.elapsed_seconds(),
                         "s total)\n");

                const double drop = base.labels > 0
                                        ? 100.0 * static_cast<double>(base.labels - aug.labels) /
                                              static_cast<double>(base.labels)
                                        : 0.0;
                const double t_drop =
                    base.seconds > 0 ? 100.0 * (base.seconds - aug.seconds) / base.seconds : 0.0;
                const bool lp_match = std::abs(base.lp_cost - aug.lp_cost) < 1e-4;  // NOLINT

                std::ostringstream lp_base_s;
                std::ostringstream lp_aug_s;
                lp_base_s << std::fixed << std::setprecision(2) << base.lp_cost;
                lp_aug_s << std::fixed << std::setprecision(2) << aug.lp_cost
                         << (lp_match ? "" : " !DIFF");
                std::ostringstream drop_s;
                std::ostringstream t_base_s;
                std::ostringstream t_aug_s;
                std::ostringstream t_drop_s;
                drop_s << std::fixed << std::setprecision(1) << drop << "%";
                t_base_s << std::fixed << std::setprecision(2) << base.seconds << "s";
                t_aug_s << std::fixed << std::setprecision(2) << aug.seconds << "s";
                t_drop_s << std::fixed << std::setprecision(1) << t_drop << "%";

                table << std::left << std::setw(8) << instance_name << std::right << std::setw(5)
                      << ng_size << std::setw(14) << lp_base_s.str() << std::setw(14)
                      << lp_aug_s.str() << std::setw(12) << base.labels << std::setw(12)
                      << aug.labels << std::setw(9) << drop_s.str() << std::setw(11)
                      << t_base_s.str() << std::setw(11) << t_aug_s.str() << std::setw(9)
                      << t_drop_s.str() << "\n";
            }
        }

        std::cout << "\n" << table.str() << std::endl;
        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception caught: ", e.what(), '\n');
        return 1;
    }
}
