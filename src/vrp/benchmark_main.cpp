// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

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

int main(int argc, char* argv[]) {
    try {
        Logger::init(LogLevel::Info);

        LOG_TRACE(__FUNCTION__, '\n');

        std::vector<std::string> instance_names;
        size_t max_instance_index = 2;
        bool run_boost = false;
        for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]) == "--boost") {
                run_boost = true;
            } else {
                max_instance_index = std::stoull(argv[i]);
            }
        }
        if (max_instance_index > 9) {  // NOLINT(readability-magic-numbers)
            LOG_ERROR(
                "Maximum instance index exceeded, should be <= 9 to match existing instances\n");
            return 1;
        }
        for (size_t instance_num = 1; instance_num <= max_instance_index; ++instance_num) {
            instance_names.emplace_back("C10" + std::to_string(instance_num));
            instance_names.emplace_back("R10" + std::to_string(instance_num));
            instance_names.emplace_back("RC10" + std::to_string(instance_num));
        }

        constexpr size_t kBucketRange = 50;
        constexpr size_t kBucketResourceIdx = 0;
        constexpr size_t kSortResourceIdx = 0;
        using BucketLC = LabelBuckets<IntResource, RealResource, ResourceType>;

        bool run_astar = false;
        for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]) == "--astar") {
                run_astar = true;
            }
        }

        std::vector<std::string> labels =
            {"Simple", "Pushing", "Pulling", "ConstructiveTabu", "Tabu", "BucketS", "BucketP"};
        if (run_astar) {
            labels.push_back("AStar");
        }
        if (run_boost) {
            labels.insert(labels.begin(), "Boost");
        }
        std::string root_dir = file_parent_dir(__FILE__, 3);

        std::vector<Timer> total_timers;
        std::vector<std::tuple<std::string, double, std::vector<Timer>>> rows;

        for (const auto& instance_name : instance_names) {
            std::string instance_path = root_dir + "/instances/" + instance_name + ".txt";

            LOG_INFO("Instance: ", instance_path, '\n');
            InstanceReader instance_reader(instance_path);
            auto instance = instance_reader.read();
            VRP vrp(instance);

            // ── Heuristic algorithms ───────────────────────────────────────────
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

            // ── LabelBuckets algorithms ────────────────────────────────────────
            BucketLC bucket_container_s(kBucketRange, kBucketResourceIdx, kSortResourceIdx);
            AlgorithmParams<BucketLC> bucket_params_s(std::move(bucket_container_s));
            auto bucket_simple =
                vrp.get_graph().create_algorithm<SimpleDominanceAlgorithm, BucketLC>(
                    bucket_params_s);

            BucketLC bucket_container_p(kBucketRange, kBucketResourceIdx, kSortResourceIdx);
            AlgorithmParams<BucketLC> bucket_params_p(std::move(bucket_container_p));
            auto bucket_pulling =
                vrp.get_graph().create_algorithm<PullingDominanceAlgorithm, BucketLC>(
                    bucket_params_p);

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
                astar_algo = vrp.get_graph().create_algorithm<AStarAlgoBound<RealResource>::Algo>(
                    astar_params);
                extra_solvers.push_back(
                    {[&vrp, algo = astar_algo.get()](const std::map<size_t, double>& dual) {
                         return vrp.run_algorithm(dual, algo);
                     },
                     true});
            }

            // ── Single CG solve ────────────────────────────────────────────────
            AlgorithmParams<LabelList<ResourceType>> list_params;
            auto [timers, lp_cost, proven_optimal] =
                vrp.solve<SimpleDominanceAlgorithm,
                          PushingDominanceAlgorithm,
                          PullingDominanceAlgorithm>(list_params,
                                                     std::nullopt,
                                                     list_algorithms,
                                                     run_boost,
                                                     extra_solvers);  // NOLINT
            if (!proven_optimal) {
                LOG_WARN("Instance ",
                         instance_name,
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
            rows.emplace_back(instance_name, lp_cost, timers);
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
