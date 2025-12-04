#pragma once

#include "rcspp/rcspp.hpp"
#include "vrp/instance.hpp"
#include "vrp/instance_reader.hpp"
#include "vrp_subproblem/vrp_subproblem.hpp"

#include <map>
#include <memory>
#include <string>

using namespace rcspp;

template <template <typename> class AlgorithmType = SimpleDominanceAlgorithm>
bool test_vrp_solve(const std::map<size_t, double>& dual_by_id, VRPSubproblem* vrp_subproblem,
  double optimal_cost) {

    auto solutions = vrp_subproblem->solve<AlgorithmType>(dual_by_id);

    if (!solutions.empty()) {
        auto cost = solutions[0].cost;
        LOG_DEBUG("cost=", cost, '\n');

        if (std::abs(cost - optimal_cost) > 1e-9) {
            LOG_ERROR("Difference with optimal cost: ", std::abs(cost - optimal_cost), ": ", cost, " vs ", optimal_cost, '\n');
            return false;
        }
    } else {
        return false;
    }

    return true;
}

template <template <typename> class AlgorithmType = SimpleDominanceAlgorithm>
bool test_rcspp() {
  // Test graph creation, graph update and solving the RCSPP

  bool success = true;

  std::string instance_name = "R101";
    std::string root_dir = file_parent_dir(__FILE__, 3);
  std::string instance_path = root_dir+"/instances/" + instance_name + ".txt";

  LOG_INFO("Instance: ", instance_path, '\n');
  InstanceReader instance_reader(instance_path);
  auto instance = instance_reader.read();

  VRPSubproblem vrp_subproblem(instance);

  // Iteration 0: Test graph creation
  const double OPTIMAL_COST_ITER_0 = -319.87786809696524415;
  std::string duals_file = "iter_0.txt";
  std::string duals_directory = root_dir+"/instances/duals/" + instance_name + "/";
  auto duals_path = duals_directory + duals_file;
  auto dual_by_id = InstanceReader::read_duals(duals_path);
  success = test_vrp_solve<AlgorithmType>(dual_by_id, &vrp_subproblem, OPTIMAL_COST_ITER_0);
    if (!success) {
        return false;
    }

  // Iteration 1: Test graph update
  const double OPTIMAL_COST_ITER_1 = -291.88751273511473983;
  duals_file = "iter_1.txt";
  duals_directory = root_dir+"/instances/duals/" + instance_name + "/";
  duals_path = duals_directory + duals_file;
  dual_by_id = InstanceReader::read_duals(duals_path);
  success = test_vrp_solve<AlgorithmType>(dual_by_id, &vrp_subproblem, OPTIMAL_COST_ITER_1);

  return success;
}

template <template <typename> class AlgorithmType = SimpleDominanceAlgorithm>
bool test_rcspp_non_integer_dual_row_coef() {
    // Test graph creation, graph update and solving the RCSPP
    // when the dual row coefficients are non-integer

    bool success = true;

    std::string instance_name = "R101";
    std::string root_dir = file_parent_dir(__FILE__, 3);
    std::string instance_path = root_dir+"/instances/" + instance_name + ".txt";

    LOG_INFO("Instance: ", instance_path, '\n');
    InstanceReader instance_reader(instance_path);
    auto instance = instance_reader.read();

    // Use a dual row coefficient of 0.5
    const double DUAL_ROW_COEF = 0.5;
    std::map<size_t, double> coef_by_id;
    for (const auto& [key, value] : instance.get_customers_by_id()) {
        coef_by_id.emplace(key, DUAL_ROW_COEF);
    }

    VRPSubproblem vrp_subproblem(instance, &coef_by_id);

    // Iteration 0: Test graph creation
    const double OPTIMAL_COST_ITER_0 = -319.87786809696524;
    std::string duals_file = "iter_0.txt";
    std::string duals_directory = root_dir+"/instances/duals/" + instance_name + "/";
    auto duals_path = duals_directory + duals_file;

    auto dual_by_id = InstanceReader::read_duals(duals_path);
    // Multiply all dual values by 2
    const double DUAL_COEF = 1 / DUAL_ROW_COEF;
    for (auto& [key, value] : dual_by_id) {
        value *= DUAL_COEF;
    }

    success = test_vrp_solve<AlgorithmType>(dual_by_id, &vrp_subproblem, OPTIMAL_COST_ITER_0);
    if (!success) {
        return false;
    }

    // Iteration 1: Test graph update
    const double OPTIMAL_COST_ITER_1 = -291.88751273511473983;
    duals_file = "iter_1.txt";
    duals_directory = root_dir+"/instances/duals/" + instance_name + "/";
    duals_path = duals_directory + duals_file;

    dual_by_id = InstanceReader::read_duals(duals_path);
    // Multiply all dual values by 2
    for (auto& [key, value] : dual_by_id) {
        value *= DUAL_COEF;
    }

    success = test_vrp_solve<AlgorithmType>(dual_by_id, &vrp_subproblem, OPTIMAL_COST_ITER_1);

    return success;
}
