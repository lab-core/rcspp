#pragma once

#include "resource/resource.h"

#include <optional>



class RealResource : public Resource<RealResource> {
  friend class RealResourceFactory;

public:

  RealResource();

  RealResource(std::unique_ptr<ExpansionFunction<RealResource>> expansion_function,
    std::unique_ptr<FeasibilityFunction<RealResource>> feasibility_function,
    std::unique_ptr<CostFunction<RealResource>> cost_function,
    std::unique_ptr<DominanceFunction<RealResource>> dominance_function);

  RealResource(double value);

  RealResource(double value, double min, double max);

  RealResource(double value, std::unique_ptr<ExpansionFunction<RealResource>> expansion_function,
    std::unique_ptr<FeasibilityFunction<RealResource>> feasibility_function,
    std::unique_ptr<CostFunction<RealResource>> cost_function,
    std::unique_ptr<DominanceFunction<RealResource>> dominance_function);

  double get_value() const;

  void set_value(double value);
  /*template<typename... Types>
  void set_value(Types...) override;*/

private:
  double value_;

};