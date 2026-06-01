#pragma once

#include <gtest/gtest.h>

#include "rcspp/rcspp.hpp"
#include "vrp/instance.hpp"
#include "vrp/instance_reader.hpp"
#include "vrp_subproblem/vrp_subproblem.hpp"

#include <map>
#include <memory>
#include <string>

using namespace rcspp;

template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
void test_vrp_solve(const std::map<size_t, double>& dual_by_id, VRPSubproblem* vrp_subproblem,
    double optimal_cost) {
    auto solutions = vrp_subproblem->solve<AlgorithmType>(dual_by_id);
    ASSERT_FALSE(solutions.empty());
    EXPECT_NEAR(solutions[0].cost, optimal_cost, 1e-9);
}

template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
void test_rcspp() {
    std::string instance_name = "R101";
    std::string root_dir = file_parent_dir(__FILE__, 3);
    std::string instance_path = root_dir + "/instances/" + instance_name + ".txt";

    InstanceReader instance_reader(instance_path);
    auto instance = instance_reader.read();
    VRPSubproblem vrp_subproblem(instance);

    std::string duals_dir = root_dir + "/instances/duals/" + instance_name + "/";

    constexpr double OPTIMAL_COST_ITER_0 = -319.87786809696524415;
    auto dual_by_id = InstanceReader::read_duals(duals_dir + "iter_0.txt");
    ASSERT_NO_FATAL_FAILURE(test_vrp_solve<AlgorithmType>(dual_by_id, &vrp_subproblem, OPTIMAL_COST_ITER_0));

    constexpr double OPTIMAL_COST_ITER_1 = -291.88751273511473983;
    dual_by_id = InstanceReader::read_duals(duals_dir + "iter_1.txt");
    test_vrp_solve<AlgorithmType>(dual_by_id, &vrp_subproblem, OPTIMAL_COST_ITER_1);
}

template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
void test_rcspp_non_integer_dual_row_coef() {
    std::string instance_name = "R101";
    std::string root_dir = file_parent_dir(__FILE__, 3);
    std::string instance_path = root_dir + "/instances/" + instance_name + ".txt";

    InstanceReader instance_reader(instance_path);
    auto instance = instance_reader.read();

    constexpr double DUAL_ROW_COEF = 0.5;
    std::map<size_t, double> coef_by_id;
    for (const auto& [key, value] : instance.get_customers_by_id()) {
        coef_by_id.emplace(key, DUAL_ROW_COEF);
    }
    VRPSubproblem vrp_subproblem(instance, &coef_by_id);

    constexpr double DUAL_COEF = 1.0 / DUAL_ROW_COEF;
    std::string duals_dir = root_dir + "/instances/duals/" + instance_name + "/";

    constexpr double OPTIMAL_COST_ITER_0 = -319.87786809696524;
    auto dual_by_id = InstanceReader::read_duals(duals_dir + "iter_0.txt");
    for (auto& [key, value] : dual_by_id) value *= DUAL_COEF;
    ASSERT_NO_FATAL_FAILURE(test_vrp_solve<AlgorithmType>(dual_by_id, &vrp_subproblem, OPTIMAL_COST_ITER_0));

    constexpr double OPTIMAL_COST_ITER_1 = -291.88751273511473983;
    dual_by_id = InstanceReader::read_duals(duals_dir + "iter_1.txt");
    for (auto& [key, value] : dual_by_id) value *= DUAL_COEF;
    test_vrp_solve<AlgorithmType>(dual_by_id, &vrp_subproblem, OPTIMAL_COST_ITER_1);
}

#define DEFINE_RCSPP_TESTS(AlgoSuffix, AlgoType)                                          \
    TEST(Rcspp_##AlgoSuffix, Iter0Iter1) { test_rcspp<AlgoType>(); }                      \
    TEST(Rcspp_##AlgoSuffix, NonIntegerDualRowCoef) {                                     \
        test_rcspp_non_integer_dual_row_coef<AlgoType>();                                 \
    }

DEFINE_RCSPP_TESTS(SimpleDominance, SimpleDominanceAlgorithm)
DEFINE_RCSPP_TESTS(PushingDominance, PushingDominanceAlgorithm)
DEFINE_RCSPP_TESTS(PullingDominance, PullingDominanceAlgorithm)

#undef DEFINE_RCSPP_TESTS