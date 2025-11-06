// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <utility>

namespace rcspp {

template <typename ResourceType>
class Resource;

template <typename ResourceType>
class FeasibilityFunction {
  public:
    virtual ~FeasibilityFunction() = default;

    virtual auto is_feasible(const Resource<ResourceType>& resource) -> bool = 0;

    [[nodiscard]] virtual auto clone() const -> std::unique_ptr<FeasibilityFunction> = 0;

    virtual auto create(const size_t node_id) -> std::unique_ptr<FeasibilityFunction> {
      auto new_feasibility_function = clone();

      new_feasibility_function->node_id_ = node_id;

      new_feasibility_function->preprocess();

      return new_feasibility_function;
    }

    virtual void reset(const size_t node_id) {
      node_id_ = node_id;

      preprocess();
    }

  protected:
    size_t node_id_ = 0;

    virtual void preprocess() {}
};
}  // namespace rcspp
