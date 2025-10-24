#include "test_rcspp.hpp"
#include "rcspp/rcspp.hpp"
#include "vrp/instance.hpp"
#include "vrp/instance_reader.hpp"
#include "vrp_subproblem/vrp_subproblem.hpp"

#include <memory>

using namespace rcspp;



bool test_rcspp() {

  bool success = true;

  const double EPSILON = 0.00000001;
  const double OPTIMAL_COST_ITER_0 = -319.87786809696524415;
  const double OPTIMAL_COST_ITER_1 = -291.88751273511473983;


  std::string instance_name = "R101";
  std::string instance_path = "../../../../instances/" + instance_name + ".txt";

  std::cout << "Instance: " << instance_path << std::endl;
  InstanceReader instance_reader(instance_path);
  auto instance = instance_reader.read();

  VRPSubproblem vrp_subproblem(instance);

  // Iteration 0
  std::string duals_file = "iter_0.txt";
  std::string duals_directory = "../../../../instances/duals/" + instance_name + "/";
  auto duals_path = duals_directory + duals_file;    
  auto dual_by_id = InstanceReader::read_duals(duals_path);
  
  auto solutions = vrp_subproblem.solve(dual_by_id);

  if (!solutions.empty()) {
      auto cost = solutions[0].cost;
      std::cout << "cost=" << cost << std::endl;

      if (std::abs(cost - OPTIMAL_COST_ITER_0) > EPSILON) {
          std::cout << "Difference with optimal cost: "
                    << std::abs(cost - OPTIMAL_COST_ITER_0) << std::endl;
          success = false;
      }
  } else {
      success = false;
  }

  // Iteration 1
  duals_file = "iter_1.txt";
  duals_directory = "../../../../instances/duals/" + instance_name + "/";
  duals_path = duals_directory + duals_file;
  dual_by_id = InstanceReader::read_duals(duals_path);

  solutions = vrp_subproblem.solve(dual_by_id);

  if (!solutions.empty()) {
      auto cost = solutions[0].cost;
      std::cout << "cost=" << cost << std::endl;

      if (std::abs(cost - OPTIMAL_COST_ITER_1) > EPSILON) {
          std::cout << "Difference with optimal cost: " << std::abs(cost - OPTIMAL_COST_ITER_1)
                    << std::endl;
          success = false;
      }
  } else {
      success = false;
  }

  return success;
}
