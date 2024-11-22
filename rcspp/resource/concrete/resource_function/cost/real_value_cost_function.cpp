#include "real_value_cost_function.h"

#include <iostream>



double RealValueCostFunction::get_cost(const RealResource& real_resource) const {

  return real_resource.get_value();
}
