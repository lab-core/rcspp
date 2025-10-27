#include "test_rcspp.hpp"
#include "rcspp/rcspp.hpp"
#include "vrp/instance.hpp"
#include "vrp/instance_reader.hpp"
#include "vrp_subproblem/vrp_subproblem.hpp"

#include <memory>

using namespace rcspp;

const double EPSILON = 0.00000001;


bool test_rcspp() {
  // Test graph creation, graph update and solving the RCSPP

  bool success = true;

  std::string instance_name = "R101";
  std::string instance_path = "../../../../instances/" + instance_name + ".txt";

  std::cout << "Instance: " << instance_path << std::endl;
  InstanceReader instance_reader(instance_path);
  auto instance = instance_reader.read();

  VRPSubproblem vrp_subproblem(instance);

  // Iteration 0: Test graph creation
  const double OPTIMAL_COST_ITER_0 = -319.87786809696524415;
  std::string duals_file = "iter_0.txt";
  std::string duals_directory = "../../../../instances/duals/" + instance_name + "/";
  auto duals_path = duals_directory + duals_file;    
  auto dual_by_id = InstanceReader::read_duals(duals_path);  
  success = test_vrp_solve(dual_by_id, &vrp_subproblem, OPTIMAL_COST_ITER_0);

  // Iteration 1: Test graph update
  const double OPTIMAL_COST_ITER_1 = -291.88751273511473983;
  duals_file = "iter_1.txt";
  duals_directory = "../../../../instances/duals/" + instance_name + "/";
  duals_path = duals_directory + duals_file;
  dual_by_id = InstanceReader::read_duals(duals_path);
  success = test_vrp_solve(dual_by_id, &vrp_subproblem, OPTIMAL_COST_ITER_1);

  return success;
}


bool test_rcspp_non_integer_dual_row_coef() {
    // Test graph creation, graph update and solving the RCSPP 
    // when the dual row coefficients are non-integer

    bool success = true;

    std::string instance_name = "R101";
    std::string instance_path = "../../../../instances/" + instance_name + ".txt";

    std::cout << "Instance: " << instance_path << std::endl;
    InstanceReader instance_reader(instance_path);
    auto instance = instance_reader.read();

    const double DUAL_ROW_COEF = 0.5;
    std::map<size_t, double> coef_by_id;
    for (auto& [key, value] : instance.get_customers_by_id()) {
        coef_by_id.emplace(key, DUAL_ROW_COEF);
    }
    
    VRPSubproblem vrp_subproblem(instance, &coef_by_id);

    // Iteration 0: Test graph creation
    const double OPTIMAL_COST_ITER_0 = -319.87786809696524415;
    std::string duals_file = "iter_0.txt";
    std::string duals_directory = "../../../../instances/duals/" + instance_name + "/";
    auto duals_path = duals_directory + duals_file;

    auto dual_by_id = InstanceReader::read_duals(duals_path);
    // Multiply all udal values by 2
    for (auto& [key, value] : dual_by_id) {
        value *= 1 / DUAL_ROW_COEF;
    }

    success = test_vrp_solve(dual_by_id, &vrp_subproblem, OPTIMAL_COST_ITER_0);

    // Iteration 1: Test graph update
    const double OPTIMAL_COST_ITER_1 = -291.88751273511473983;
    duals_file = "iter_1.txt";
    duals_directory = "../../../../instances/duals/" + instance_name + "/";
    duals_path = duals_directory + duals_file;

    dual_by_id = InstanceReader::read_duals(duals_path);
    // Multiply all udal values by 2
    for (auto& [key, value] : dual_by_id) {
        value *= 1 / DUAL_ROW_COEF;
    }

    success = test_vrp_solve(dual_by_id, &vrp_subproblem, OPTIMAL_COST_ITER_1);

    return success;
}

bool test_vrp_solve(const std::map<size_t, double>& dual_by_id, VRPSubproblem* vrp_subproblem, 
  double optimal_cost) {

    bool success = true;

    auto solutions = vrp_subproblem->solve(dual_by_id);

    if (!solutions.empty()) {
        auto cost = solutions[0].cost;
        std::cout << "cost=" << cost << std::endl;

        if (std::abs(cost - optimal_cost) > EPSILON) {
            std::cout << "Difference with optimal cost: " << std::abs(cost - optimal_cost)
                      << std::endl;
            success = false;
        }
    } else {
        success = false;
    }

    return success;
}
