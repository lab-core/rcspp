#pragma once

#include "resource/concrete_resource.h"

#include <optional>



class RealResource : public ConcreteResource<RealResource> {
  friend class RealResourceFactory;

public:

  RealResource(double value);

  RealResource(std::unique_ptr<ExpansionFunction> expansion_function, 
    std::unique_ptr<FeasibilityFunction> feasibility_function,
    std::unique_ptr<CostFunction> cost_function, 
    std::unique_ptr<DominanceFunction> dominance_function, 
    double value);

  double get_value() const;

  void set_value(double value);

private:
  double value_;

};