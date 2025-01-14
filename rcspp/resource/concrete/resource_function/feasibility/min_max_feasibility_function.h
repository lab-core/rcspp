#pragma once

#include "resource/resource_function/feasibility/feasibility_function.h"
#include "resource/concrete/real_resource.h"
#include "general/clonable.h"


class MinMaxFeasibilityFunction : public Clonable<MinMaxFeasibilityFunction, FeasibilityFunction<RealResource>> {

private:
  double min_;
  double max_;

public:

  MinMaxFeasibilityFunction(double min, double max) : min_(min), max_(max) {}

  bool is_feasible(const RealResource& resource) override;

};