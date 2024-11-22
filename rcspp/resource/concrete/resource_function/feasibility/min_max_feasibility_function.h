#pragma once

#include "resource/resource_function/feasibility/resource_feasibility_function.h"
#include "resource/concrete/real_resource.h"


class MinMaxFeasibilityFunction : public ResourceFeasibilityFunction<MinMaxFeasibilityFunction, RealResource> {

private:
  double min_;
  double max_;

public:

  MinMaxFeasibilityFunction(double min, double max) : min_(min), max_(max) {}

  bool is_feasible(const RealResource& resource) override;

};