// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <optional>

#include "rcspp/resource/base/resource_base.hpp"

namespace rcspp {

class RealResource : public ResourceBase<RealResource> {
    friend class RealResourceFactory;

  public:
    RealResource();

    explicit RealResource(double value);

    [[nodiscard]] auto get_value() const -> double;

    void set_value(double value);

    void reset() override;

  private:
    double value_;
};
}  // namespace rcspp
