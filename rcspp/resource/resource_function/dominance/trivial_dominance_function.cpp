#include "resource/resource_function/dominance/trivial_dominance_function.h"
#include "resource/resource.h"

#include <memory>


bool TrivialDominanceFunction::check_dominance(const Resource& lhs_resource, const Resource& rhs_resource) {

  return true;
}
