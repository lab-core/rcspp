#include "resource/concrete/resource_function/feasibility/min_max_feasibility_function.h"


bool MinMaxFeasibilityFunction::is_feasible(const RealResource& resource) {

  bool feasible = true;

  if ((resource.get_value() < min_) || (resource.get_value() > max_)) {
    feasible = false;
  }

  return feasible;
}
