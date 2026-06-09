// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

/// @brief Large-instance benchmark for Solomon C2/R2/RC2 and Gehring & Homberger families.
///
/// Usage:
///   rcspp-vrp-benchmark-large [max_c2_rc2] [--r2-max N] [--gh-dir path/to/gh/instances]
///
/// Defaults: max_c2_rc2=2, r2-max=same as max_c2_rc2.
/// C2 and RC2 have 8 instances each (C201–C208, RC201–RC208).
/// R2 has 11 instances (R201–R211); use --r2-max 11 to run all.
///
/// Gehring & Homberger 200–1000-customer instances are not distributed with this repo.
/// Point --gh-dir at a directory of .txt files in Solomon format to include them.

#include <filesystem>
#include <iostream>

#ifdef RCSPP_VRP_HAS_BOOST
#include "cg/subproblem/boost/boost_subproblem.hpp"
#endif
#include "benchmark_common.hpp"
#include "instance.hpp"
#include "instance_reader.hpp"
#include "rcspp/algorithm/diversification_search.hpp"
#include "rcspp/algorithm/greedy.hpp"
#include "rcspp/rcspp.hpp"
#include "solution_output.hpp"
#include "vrp.hpp"

namespace fs = std::filesystem;

static CGSolveResult run_vrp(const std::string& instance_path, bool run_boost, bool run_astar) {
    InstanceReader reader(instance_path);
    auto instance = reader.read();
    VRP vrp(instance);

    constexpr size_t kBucketRange = 50;
    constexpr size_t kBucketResourceIdx = 0;
    constexpr size_t kSortResourceIdx = 0;
    using BucketLC = LabelBuckets<IntResource, RealResource, ResourceType>;

    // ── Heuristic algorithms ───────────────────────────────────────────────────
    AlgorithmParams<LabelList<ResourceType>> constructive_tabu_params;
    constructive_tabu_params.stop_after_X_solutions = 20;  // NOLINT
    constructive_tabu_params.max_iterations = 1e6;         // NOLINT
    auto constructive_tabu_algo =
        vrp.get_graph().create_algorithm<DiversificationSearch>(constructive_tabu_params);

    AlgorithmParams<LabelList<ResourceType>> tabu_params;
    tabu_params.stop_after_X_solutions = 20;  // NOLINT(readability-magic-numbers)
    tabu_params.max_iterations = 1e3;         // NOLINT(readability-magic-numbers)
    auto tabu_algo = vrp.get_graph().create_algorithm<ImprovingTabuSearch>(tabu_params);

    std::vector<Algorithm<ResourceType, LabelList<ResourceType>>*> list_algorithms = {
        constructive_tabu_algo.get(),
        tabu_algo.get()};

    // ── Bucket algorithms ──────────────────────────────────────────────────────
    BucketLC bucket_container_s(kBucketRange, kBucketResourceIdx, kSortResourceIdx);
    AlgorithmParams<BucketLC> bucket_params_s(std::move(bucket_container_s));
    auto bucket_simple =
        vrp.get_graph().create_algorithm<SimpleDominanceAlgorithm, BucketLC>(bucket_params_s);

    BucketLC bucket_container_p(kBucketRange, kBucketResourceIdx, kSortResourceIdx);
    AlgorithmParams<BucketLC> bucket_params_p(std::move(bucket_container_p));
    auto bucket_pulling =
        vrp.get_graph().create_algorithm<PullingDominanceAlgorithm, BucketLC>(bucket_params_p);

    std::vector<ExtraSolver> extra_solvers = {
        {[&vrp, algo = bucket_simple.get()](const std::map<size_t, double>& dual) {
             return vrp.run_algorithm(dual, algo);
         },
         true},
        {[&vrp, algo = bucket_pulling.get()](const std::map<size_t, double>& dual) {
             return vrp.run_algorithm(dual, algo);
         },
         true}};

    std::unique_ptr<Algorithm<ResourceType, LabelList<ResourceType>>> astar_algo;
    if (run_astar) {
        AlgorithmParams<LabelList<ResourceType>> astar_params;
        astar_algo =
            vrp.get_graph().create_algorithm<AStarAlgoBound<RealResource>::Algo>(astar_params);
        extra_solvers.push_back(
            {[&vrp, algo = astar_algo.get()](const std::map<size_t, double>& dual) {
                 return vrp.run_algorithm(dual, algo);
             },
             true});
    }

    AlgorithmParams<LabelList<ResourceType>> list_params;
    return vrp
        .solve<SimpleDominanceAlgorithm, PushingDominanceAlgorithm, PullingDominanceAlgorithm>(
            list_params,
            std::nullopt,
            list_algorithms,
            run_boost,
            extra_solvers);  // NOLINT
}

int main(int argc, char* argv[]) {  // NOLINT
    try {
        Logger::init(LogLevel::Info);

        size_t max_c2_rc2 = 2;
        size_t max_r2 = 0;  // 0 = same as max_c2_rc2
        bool run_boost = false;
        bool run_astar = false;
        std::string gh_dir;

        for (int i = 1; i < argc; ++i) {
            std::string arg(argv[i]);
            if (arg == "--boost") {
                run_boost = true;
            } else if (arg == "--astar") {
                run_astar = true;
            } else if (arg == "--r2-max" && i + 1 < argc) {
                max_r2 = std::stoull(argv[++i]);
            } else if (arg == "--gh-dir" && i + 1 < argc) {
                gh_dir = argv[++i];
            } else {
                max_c2_rc2 = std::stoull(arg);
            }
        }
        if (max_r2 == 0) {
            max_r2 = max_c2_rc2;
        }

        constexpr size_t kMaxC2RC2 = 8;
        constexpr size_t kMaxR2 = 11;
        if (max_c2_rc2 > kMaxC2RC2) {
            LOG_ERROR("max_c2_rc2 exceeds 8 (C2 and RC2 only have 8 instances each)\n");
            return 1;
        }
        if (max_r2 > kMaxR2) {
            LOG_ERROR("r2-max exceeds 11 (R2 has 11 instances: R201–R211)\n");
            return 1;
        }

        std::string root_dir = file_parent_dir(__FILE__, 3);
        std::string inst_dir = root_dir + "/instances/";

        // Solomon set-2 instances use a 2-digit index: C201..C208, RC201..RC208, R201..R211.
        // Zero-pad so n >= 10 (R210, R211) is built correctly instead of "R2010"/"R2011".
        auto pad2 = [](size_t n) { return (n < 10 ? "0" : "") + std::to_string(n); };
        std::vector<std::string> instance_names;
        for (size_t n = 1; n <= max_c2_rc2; ++n) {
            instance_names.emplace_back("C2" + pad2(n));
            instance_names.emplace_back("RC2" + pad2(n));
        }
        for (size_t n = 1; n <= max_r2; ++n) {
            instance_names.emplace_back("R2" + pad2(n));
        }

        // Optional Gehring & Homberger instances (200–1000 customers, Solomon format)
        std::vector<std::string> gh_instance_paths;
        if (!gh_dir.empty()) {
            if (!fs::is_directory(gh_dir)) {
                LOG_ERROR("--gh-dir '", gh_dir, "' is not a directory\n");
                return 1;
            }
            for (const auto& entry : fs::directory_iterator(gh_dir)) {
                if (entry.path().extension() == ".txt") {
                    gh_instance_paths.push_back(entry.path().string());
                }
            }
            std::sort(gh_instance_paths.begin(), gh_instance_paths.end());
        }

        std::vector<std::string> labels =
            {"Simple", "Pushing", "Pulling", "ConstructiveTabu", "Tabu", "BucketS", "BucketP"};
        if (run_astar) {
            labels.emplace_back("AStar");
        }
        if (run_boost) {
            labels.insert(labels.begin(), "Boost");
        }

        std::vector<Timer> total_timers;
        std::vector<std::tuple<std::string, double, std::vector<Timer>>> rows;

        // Solomon C2/RC2/R2
        for (const auto& name : instance_names) {
            std::string path = inst_dir + name + ".txt";
            LOG_INFO("Instance: ", path, '\n');
            auto [timers, lp_cost, proven_optimal] = run_vrp(path, run_boost, run_astar);
            if (!proven_optimal) {
                LOG_WARN("Instance ",
                         name,
                         ": LP cost ",
                         lp_cost,
                         " is NOT proven optimal (pricing subproblem was cut short).\n");
            }
            if (total_timers.empty()) {
                total_timers = timers;
            } else {
                for (size_t i = 0; i < timers.size(); ++i) {
                    total_timers[i] += timers[i];
                }
            }
            rows.emplace_back(name, lp_cost, timers);
        }

        // Gehring & Homberger
        for (const auto& path : gh_instance_paths) {
            std::string name = fs::path(path).stem().string();
            LOG_INFO("Instance: ", path, '\n');
            auto [timers, lp_cost, proven_optimal] = run_vrp(path, run_boost, run_astar);
            if (!proven_optimal) {
                LOG_WARN("Instance ",
                         name,
                         ": LP cost ",
                         lp_cost,
                         " is NOT proven optimal (pricing subproblem was cut short).\n");
            }
            if (total_timers.empty()) {
                total_timers = timers;
            } else {
                for (size_t i = 0; i < timers.size(); ++i) {
                    total_timers[i] += timers[i];
                }
            }
            rows.emplace_back(name, lp_cost, timers);
        }

        if (rows.empty()) {
            LOG_WARN("No instances were run. Use a positive index or --gh-dir.\n");
            return 0;
        }

        auto table = format_benchmark_table(rows, labels, total_timers);
        LOG_INFO('\n', std::string(80, '='), '\n', table, std::string(80, '='), '\n');

        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception caught: ", e.what(), '\n');
        return 1;
    } catch (...) {
        LOG_ERROR("Unknown exception caught\n");
        return 1;
    }
}
