#include "real_addition_expansion_function.h"

void RealAdditionExpansionFunction::expand(const RealResource &lhs_resource,
                                           const RealResource &rhs_resource,
                                           RealResource &expanded_resource) {

  auto sum_value = lhs_resource.get_value() + rhs_resource.get_value();

  expanded_resource.set_value(sum_value);
}
