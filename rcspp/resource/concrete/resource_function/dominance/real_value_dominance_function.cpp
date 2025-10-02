#include "real_value_dominance_function.h"

bool RealValueDominanceFunction::check_dominance(
    const RealResource &lhs_resource, const RealResource &rhs_resource) {

  return lhs_resource.get_value() <= rhs_resource.get_value();
}
