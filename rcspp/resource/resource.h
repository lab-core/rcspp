#pragma once

#include <memory>

class ExpansionFunction;
class FeasibilityFunction;
class CostFunction;
class DominanceFunction;


class Resource {

public:

  Resource();

  Resource(std::unique_ptr<ExpansionFunction> expansion_function, std::unique_ptr<FeasibilityFunction> feasibility_function,
    std::unique_ptr<CostFunction> cost_function, std::unique_ptr<DominanceFunction> dominance_function);

  Resource(const Resource& rhs_resource);

  virtual ~Resource();

  Resource& operator=(const Resource& rhs_resource);

  //! Check dominance
  virtual bool operator<=(const Resource& rhs_resource) const;

  //! Resource expansion
  void expand(const Resource& rhs_resource, Resource& expanded_resource) const;

  //! Return resource cost
  virtual double get_cost() const;

  //! Return true if the resource is feasible
  virtual bool is_feasible() const;

  virtual std::unique_ptr<Resource> clone() const = 0;

protected:
  std::unique_ptr<ExpansionFunction> expansion_function_;
  std::unique_ptr<FeasibilityFunction> feasibility_function_;
  std::unique_ptr<CostFunction> cost_function_;
  std::unique_ptr<DominanceFunction> dominance_function_;

};