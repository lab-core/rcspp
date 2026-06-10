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
//   max_instance_index : 1..9, expands to C10i / R10i / RC10i (default 2)
//   --ng s1 s2 ...     : ng-neighbourhood sizes to sweep (default 3 5 8)

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

// Single pricing solve with zero duals: returns the best reduced cost found and
// the number of labels extended (a proxy for the labeling effort / dominance
// strength).  Builds a fresh VRP so the measurement is independent of CG state.
struct PricingStats {
        double best_cost = std::numeric_limits<double>::infinity();
        size_t extended = 0;
};

PricingStats price_once(const std::string& instance_name, const Instance& instance, size_t ng_size,
                        bool augment) {
    const std::string tag = run_tag(instance_name, ng_size, augment);
    LOG_INFO(tag, " pricing solve start\n");
    Timer timer;
    timer.start();

    VRP vrp(instance, ng_size, augment);
    std::map<size_t, double> zero_dual;
    for (size_t id : instance.get_demand_customers_id()) {
        zero_dual[id] = 0.0;
    }
    auto algo = vrp.get_graph().create_algorithm<SimpleDominanceAlgorithm, LabelList<ResourceType>>(
        AlgorithmParams<LabelList<ResourceType>>());
    auto solutions = vrp.run_algorithm(zero_dual, algo.get());

    PricingStats stats;
    stats.extended = algo->num_extended_labels();
    for (const auto& sol : solutions) {
        stats.best_cost = std::min(stats.best_cost, sol.cost);
    }

    timer.stop();
    LOG_INFO(tag,
             " pricing solve done  labels=",
             stats.extended,
             " best_rc=",
             stats.best_cost,
             " (",
             timer.elapsed_seconds(),
             "s)\n");
    return stats;
}

// Full column generation: returns the final LP relaxation cost.
double cg_lp_cost(const std::string& instance_name, const Instance& instance, size_t ng_size,
                  bool augment) {
    const std::string tag = run_tag(instance_name, ng_size, augment);
    LOG_INFO(tag, " CG LP solve   start\n");
    Timer timer;
    timer.start();

    VRP vrp(instance, ng_size, augment);
    auto result = vrp.solve<SimpleDominanceAlgorithm>(AlgorithmParams<LabelList<ResourceType>>());

    timer.stop();
    LOG_INFO(tag,
             " CG LP solve   done  lp=",
             result.lp_cost,
             " (",
             timer.elapsed_seconds(),
             "s)\n");
    return result.lp_cost;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        Logger::init(LogLevel::Info);

        size_t max_instance_index = 2;
        std::vector<size_t> ng_sizes;
        bool reading_ng = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--ng") {
                reading_ng = true;
            } else if (reading_ng) {
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

        std::vector<std::string> instance_names;
        for (size_t i = 1; i <= max_instance_index; ++i) {
            instance_names.emplace_back("C10" + std::to_string(i));
            instance_names.emplace_back("R10" + std::to_string(i));
            instance_names.emplace_back("RC10" + std::to_string(i));
        }

        const std::string root_dir = file_parent_dir(__FILE__, 3);

        std::ostringstream table;
        table << std::left << std::setw(8) << "Instance" << std::right << std::setw(5) << "ng"
              << std::setw(14) << "lp_base" << std::setw(14) << "lp_aug" << std::setw(12)
              << "nlab_base" << std::setw(12) << "nlab_aug" << std::setw(9) << "drop%" << "\n";
        table << std::string(74, '-') << "\n";

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

                const double lp_base = cg_lp_cost(instance_name, instance, ng_size, false);
                const double lp_aug = cg_lp_cost(instance_name, instance, ng_size, true);
                const PricingStats base = price_once(instance_name, instance, ng_size, false);
                const PricingStats aug = price_once(instance_name, instance, ng_size, true);

                test_timer.stop();
                LOG_INFO("Test done : ",
                         header,
                         "  lp_base=",
                         lp_base,
                         " lp_aug=",
                         lp_aug,
                         " nlab_base=",
                         base.extended,
                         " nlab_aug=",
                         aug.extended,
                         "  (",
                         test_timer.elapsed_seconds(),
                         "s total)\n");

                const double drop =
                    base.extended > 0 ? 100.0 * static_cast<double>(base.extended - aug.extended) /
                                            static_cast<double>(base.extended)
                                      : 0.0;
                const bool lp_match = std::abs(lp_base - lp_aug) < 1e-4;  // NOLINT

                std::ostringstream lp_base_s;
                std::ostringstream lp_aug_s;
                lp_base_s << std::fixed << std::setprecision(2) << lp_base;
                lp_aug_s << std::fixed << std::setprecision(2) << lp_aug
                         << (lp_match ? "" : " !DIFF");
                std::ostringstream drop_s;
                drop_s << std::fixed << std::setprecision(1) << drop << "%";

                table << std::left << std::setw(8) << instance_name << std::right << std::setw(5)
                      << ng_size << std::setw(14) << lp_base_s.str() << std::setw(14)
                      << lp_aug_s.str() << std::setw(12) << base.extended << std::setw(12)
                      << aug.extended << std::setw(9) << drop_s.str() << "\n";
            }
        }

        std::cout << "\n" << table.str() << std::endl;
        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception caught: ", e.what(), '\n');
        return 1;
    }
}
